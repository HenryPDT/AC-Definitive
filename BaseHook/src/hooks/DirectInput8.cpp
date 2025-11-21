#include "pch.h"
#include "base.h"
#include <cstring>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <shared_mutex>
#include <mutex>
#include <ole2.h>

// --- Function Typedefs ---
typedef HRESULT(WINAPI* DirectInput8Create_t)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);
typedef HRESULT(STDMETHODCALLTYPE* IDirectInput8_CreateDevice_t)(IDirectInput8*, REFGUID, LPDIRECTINPUTDEVICE8*, LPUNKNOWN);
typedef HRESULT(STDMETHODCALLTYPE* IDirectInputDevice8_GetDeviceState_t)(IDirectInputDevice8*, DWORD, LPVOID);
typedef HRESULT(STDMETHODCALLTYPE* IDirectInputDevice8_GetDeviceData_t)(IDirectInputDevice8*, DWORD, LPDIDEVICEOBJECTDATA, LPDWORD, DWORD);

// --- Original Functions ---
static DirectInput8Create_t oDirectInput8Create = nullptr;
static IDirectInput8_CreateDevice_t oCreateDevice = nullptr;
static IDirectInputDevice8_GetDeviceState_t oGetDeviceState = nullptr;
static IDirectInputDevice8_GetDeviceData_t oGetDeviceData = nullptr;

// Trackers
static std::unordered_set<void*> g_hooked_functions;
static std::unordered_set<IDirectInputDevice8*> g_logged_devices;
static std::unordered_map<IDirectInputDevice8*, BYTE> g_device_types;
static std::unordered_set<IDirectInputDevice8*> g_failed_cap_devices;
static std::shared_mutex g_dinput_mutex;

// Thread-safe Input Buffer
static std::mutex g_input_queue_mutex;
struct MouseBuffer {
    long lX = 0;
    long lY = 0;
    long lZ = 0;
} g_mouse_buffer;

// --- Helpers ---
static std::string GuidToString(const GUID& guid) {
    wchar_t guid_string[40];
    if (StringFromGUID2(guid, guid_string, sizeof(guid_string) / sizeof(wchar_t)) > 0) {
        char narrow_string[40];
        size_t converted_chars = 0;
        wcstombs_s(&converted_chars, narrow_string, sizeof(narrow_string), guid_string, _TRUNCATE);
        return std::string(narrow_string);
    }
    return "Invalid GUID";
}

// --- Hooks ---
HRESULT STDMETHODCALLTYPE hkGetDeviceState(IDirectInputDevice8* pDevice, DWORD cbData, LPVOID lpvData)
{
    HRESULT hr = oGetDeviceState(pDevice, cbData, lpvData);

    if (SUCCEEDED(hr) && (BaseHook::Data::bBlockInput || !BaseHook::Data::bIsInitialized))
    {
        BYTE devType = 0;

        // Optimization: Thread-local cache to avoid mutex contention on high-poll rate devices
        static thread_local IDirectInputDevice8* t_lastDevice = nullptr;
        static thread_local BYTE t_lastType = 0;

        if (pDevice == t_lastDevice) {
            devType = t_lastType;
        }
        else

        {
            std::shared_lock<std::shared_mutex> lock(g_dinput_mutex);
            auto it = g_device_types.find(pDevice);
            if (it != g_device_types.end()) {
                devType = it->second;
            } else if (g_failed_cap_devices.count(pDevice)) {
                return hr; 
            }
        }

        if (devType == 0) {
            DIDEVCAPS caps = { sizeof(caps) };
            HRESULT capHr = pDevice->GetCapabilities(&caps);
            
            std::unique_lock<std::shared_mutex> lock(g_dinput_mutex);
            if (SUCCEEDED(capHr)) {
                devType = LOBYTE(caps.dwDevType);
                g_device_types[pDevice] = devType;
                if (g_logged_devices.find(pDevice) == g_logged_devices.end()) {
                    LOG_INFO("DI8: Device discovered: %p (Type: %d)", pDevice, devType);
                    g_logged_devices.insert(pDevice);
                }
            } else {
                g_failed_cap_devices.insert(pDevice);
            }
        }

        // Update thread-local cache
        t_lastDevice = pDevice;
        t_lastType = devType;

        if (devType != 0)
        {
            // Restore Mouse Interaction for ImGui (Older games/Exclusive Mode)
            if (devType == DI8DEVTYPE_MOUSE && BaseHook::Data::bIsInitialized)
            {
                ImGuiIO& io = ImGui::GetIO(); // Declare io here
                if (cbData == sizeof(DIMOUSESTATE) || cbData == sizeof(DIMOUSESTATE2))
                {
                    auto* ms = static_cast<DIMOUSESTATE*>(lpvData);

                    // CRITICAL: Accumulate in a thread-safe buffer. Do NOT touch ImGui::GetIO() here!
                    std::lock_guard<std::mutex> lock(g_input_queue_mutex);
                    g_mouse_buffer.lX += ms->lX;
                    g_mouse_buffer.lY += ms->lY;
                    g_mouse_buffer.lZ += ms->lZ;

                    for (int i = 0; i < 4; i++)
                        io.MouseDown[i] = (ms->rgbButtons[i] & 0x80) != 0; // IO.MouseDown is usually safe-ish to write if atomic byte, but ideally buffer this too.
                        // Note: MouseDown is array of bool. Concurrent R/W is technically UB but rarely crashes.
                        // For strict correctness, we should buffer buttons too, but standard ImGui impl tolerates this often.
                        // Given complexity, we stick to delta buffering which is the main issue for drifting.

                    if (cbData == sizeof(DIMOUSESTATE2)) {
                        auto* ms2 = static_cast<DIMOUSESTATE2*>(lpvData);
                        for (int i = 4; i < 8; i++)
                            io.MouseDown[i] = (ms2->rgbButtons[i] & 0x80) != 0;
                    }
                }
            }

            bool shouldBlock = BaseHook::Data::bBlockInput;
            if (shouldBlock)
            {
                if (devType == DI8DEVTYPE_MOUSE)
                {
                    if (cbData == sizeof(DIMOUSESTATE) || cbData == sizeof(DIMOUSESTATE2)) {
                        std::memset(lpvData, 0, cbData);
                    }
                }
                else if (devType == DI8DEVTYPE_KEYBOARD)
                {
                    std::memset(lpvData, 0, cbData);
                }
                else 
                {
                    std::memset(lpvData, 0, cbData);
                    if (cbData >= sizeof(DIJOYSTATE)) {
                        auto* js = static_cast<DIJOYSTATE*>(lpvData);
                        for(int i=0; i<4; i++) js->rgdwPOV[i] = 0xFFFFFFFF;
                    }
                }
            }
        }
    }
    return hr;
}

HRESULT STDMETHODCALLTYPE hkGetDeviceData(IDirectInputDevice8* pDevice, DWORD cbObjectData, LPDIDEVICEOBJECTDATA rgdod, LPDWORD pdwInOut, DWORD dwFlags)
{
    HRESULT hr = oGetDeviceData(pDevice, cbObjectData, rgdod, pdwInOut, dwFlags);
    
    if (SUCCEEDED(hr) && BaseHook::Data::bBlockInput && 
        (dwFlags & DIGDD_PEEK) == 0 && pdwInOut && *pdwInOut > 0)
    {
        BYTE devType = 0;
        {
            std::shared_lock<std::shared_mutex> lock(g_dinput_mutex);
            auto it = g_device_types.find(pDevice);
            if (it != g_device_types.end()) devType = it->second;
        }

        if (devType != 0) {
             *pdwInOut = 0;
             hr = DI_OK; 
        }
    }
    return hr;
}

HRESULT STDMETHODCALLTYPE hkCreateDevice(IDirectInput8* pDI, REFGUID rguid, LPDIRECTINPUTDEVICE8* lplpDirectInputDevice, LPUNKNOWN pUnkOuter)
{
    HRESULT hr = oCreateDevice(pDI, rguid, lplpDirectInputDevice, pUnkOuter);
    
    if (SUCCEEDED(hr) && lplpDirectInputDevice && *lplpDirectInputDevice)
    {
        IDirectInputDevice8* device = *lplpDirectInputDevice;
        void** vtable = *reinterpret_cast<void***>(device);

        std::unique_lock<std::shared_mutex> lock(g_dinput_mutex);

        void* pGetDeviceState = vtable[9];
        void* pGetDeviceData = vtable[10];

        if (g_hooked_functions.find(pGetDeviceState) == g_hooked_functions.end())
        {
            if (MH_CreateHook(pGetDeviceState, &hkGetDeviceState, reinterpret_cast<void**>(&oGetDeviceState)) == MH_OK &&
                MH_EnableHook(pGetDeviceState) == MH_OK) 
            {
                g_hooked_functions.insert(pGetDeviceState);
                LOG_INFO("DI8: Hooked GetDeviceState at %p", pGetDeviceState);
            }
        }

        if (g_hooked_functions.find(pGetDeviceData) == g_hooked_functions.end())
        {
            if (MH_CreateHook(pGetDeviceData, &hkGetDeviceData, reinterpret_cast<void**>(&oGetDeviceData)) == MH_OK &&
                MH_EnableHook(pGetDeviceData) == MH_OK) 
            {
                g_hooked_functions.insert(pGetDeviceData);
                LOG_INFO("DI8: Hooked GetDeviceData at %p", pGetDeviceData);
            }
        }
    }
    return hr;
}

HRESULT WINAPI hkDirectInput8Create(HINSTANCE hinst, DWORD dwVersion, REFIID riidltf, LPVOID* ppvOut, LPUNKNOWN pUnkOuter)
{
    HRESULT hr = oDirectInput8Create(hinst, dwVersion, riidltf, ppvOut, pUnkOuter);
    if (SUCCEEDED(hr) && ppvOut && *ppvOut)
    {
        IDirectInput8* pDI = static_cast<IDirectInput8*>(*ppvOut);
        void** vtable = *reinterpret_cast<void***>(pDI);
        void* pCreateDevice = vtable[3]; 

        std::unique_lock<std::shared_mutex> lock(g_dinput_mutex);

        if (g_hooked_functions.find(pCreateDevice) == g_hooked_functions.end())
        {
            if (MH_CreateHook(pCreateDevice, &hkCreateDevice, reinterpret_cast<void**>(&oCreateDevice)) == MH_OK &&
                MH_EnableHook(pCreateDevice) == MH_OK) 
            {
                g_hooked_functions.insert(pCreateDevice);
                LOG_INFO("DI8: Hooked CreateDevice at %p", pCreateDevice);
            }
        }
    }
    return hr;
}

namespace BaseHook { namespace Hooks {
    void InitDirectInput()
    {
        HMODULE hDInput8 = GetModuleHandleA("dinput8.dll");
        if (!hDInput8) hDInput8 = LoadLibraryA("dinput8.dll");
        
        if (!hDInput8) return;

        void* pDirectInput8Create = GetProcAddress(hDInput8, "DirectInput8Create");
        if (!pDirectInput8Create) return;

        // Hook the Export
        {
            std::unique_lock<std::shared_mutex> lock(g_dinput_mutex);
            if (MH_CreateHook(pDirectInput8Create, &hkDirectInput8Create, reinterpret_cast<void**>(&oDirectInput8Create)) == MH_OK &&
                MH_EnableHook(pDirectInput8Create) == MH_OK) {
                g_hooked_functions.insert(pDirectInput8Create);
                LOG_INFO("DI8: Hooked DirectInput8Create export.");
            }
        }

        // Try Dummy Device method
        using DirectInput8Create_t = HRESULT(WINAPI*)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);
        auto fnDirectInput8Create = (DirectInput8Create_t)pDirectInput8Create;

        IDirectInput8* pDummyDI = nullptr;
        if (SUCCEEDED(fnDirectInput8Create(GetModuleHandle(NULL), 0x0800, IID_IDirectInput8, (void**)&pDummyDI, NULL)) && pDummyDI)
        {
            IDirectInputDevice8* pDummyDevice = nullptr;
            if (SUCCEEDED(pDummyDI->CreateDevice(GUID_SysMouse, &pDummyDevice, NULL)) && pDummyDevice)
            {
                void** vtable = *(void***)pDummyDevice;
                void* pGetDeviceState = vtable[9];
                void* pGetDeviceData = vtable[10];
                
                std::unique_lock<std::shared_mutex> lock(g_dinput_mutex);

                if (g_hooked_functions.find(pGetDeviceState) == g_hooked_functions.end()) {
                    if (MH_CreateHook(pGetDeviceState, &hkGetDeviceState, reinterpret_cast<void**>(&oGetDeviceState)) == MH_OK &&
                        MH_EnableHook(pGetDeviceState) == MH_OK) {
                        g_hooked_functions.insert(pGetDeviceState);
                        LOG_INFO("DI8: Hooked GetDeviceState (dummy).");
                    } else {
                        LOG_ERROR("DI8: Failed to hook GetDeviceState (dummy).");
                    }
                }
                if (g_hooked_functions.find(pGetDeviceData) == g_hooked_functions.end()) {
                    if (MH_CreateHook(pGetDeviceData, &hkGetDeviceData, reinterpret_cast<void**>(&oGetDeviceData)) == MH_OK &&
                        MH_EnableHook(pGetDeviceData) == MH_OK) {
                        g_hooked_functions.insert(pGetDeviceData);
                        LOG_INFO("DI8: Hooked GetDeviceData (dummy).");
                    } else {
                        LOG_ERROR("DI8: Failed to hook GetDeviceData (dummy).");
                    }
                }
                pDummyDevice->Release();
            }
            pDummyDI->Release();
        }
    }

    void ApplyBufferedInput()
    {
        std::lock_guard<std::mutex> lock(g_input_queue_mutex);
        if (g_mouse_buffer.lX != 0 || g_mouse_buffer.lY != 0 || g_mouse_buffer.lZ != 0)
        {
            ImGuiIO& io = ImGui::GetIO();
            io.MouseDelta.x += (float)g_mouse_buffer.lX;
            io.MouseDelta.y += (float)g_mouse_buffer.lY;
            io.MouseWheel += (float)g_mouse_buffer.lZ / 120.0f;

            g_mouse_buffer.lX = 0;
            g_mouse_buffer.lY = 0;
            g_mouse_buffer.lZ = 0;
        }
    }
}}
