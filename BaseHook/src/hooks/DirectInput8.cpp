#include "pch.h"
#include "base.h"
#include <cstring>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <mutex>
#include <ole2.h>

// --- Function Typedefs ---
typedef HRESULT(WINAPI* DirectInput8Create_t)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);
typedef HRESULT(STDMETHODCALLTYPE* IDirectInput8_CreateDevice_t)(IDirectInput8*, REFGUID, LPDIRECTINPUTDEVICE8*, LPUNKNOWN);
typedef HRESULT(STDMETHODCALLTYPE* IDirectInputDevice8_GetDeviceState_t)(IDirectInputDevice8*, DWORD, LPVOID);
typedef HRESULT(STDMETHODCALLTYPE* IDirectInputDevice8_GetDeviceData_t)(IDirectInputDevice8*, DWORD, LPDIDEVICEOBJECTDATA, LPDWORD, DWORD);

// --- Original Functions (Trampolines) ---
static DirectInput8Create_t oDirectInput8Create = nullptr;
static IDirectInput8_CreateDevice_t oCreateDevice = nullptr;

static IDirectInputDevice8_GetDeviceState_t oGetDeviceState = nullptr;
static IDirectInputDevice8_GetDeviceData_t oGetDeviceData = nullptr;

// Keep track of hooked function pointers to avoid redundant hooks
static std::unordered_set<void*> g_hooked_functions;
// Keep track of devices we've logged to avoid log spam
static std::unordered_set<IDirectInputDevice8*> g_logged_devices;
// Cache device types to avoid calling GetCapabilities every frame
static std::unordered_map<IDirectInputDevice8*, BYTE> g_device_types;
// Mutex for thread safety
static std::mutex g_dinput_mutex;

// Helper to convert GUID to string for logging
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

// --- Unified Hook Implementations ---
HRESULT STDMETHODCALLTYPE hkGetDeviceState(IDirectInputDevice8* pDevice, DWORD cbData, LPVOID lpvData)
{
    HRESULT hr = oGetDeviceState(pDevice, cbData, lpvData);

    if (SUCCEEDED(hr))
    {
        BYTE devType = 0;

        // Thread-safe cache lookup
        {
            std::lock_guard<std::mutex> lock(g_dinput_mutex);
            auto it = g_device_types.find(pDevice);
            if (it != g_device_types.end()) {
                devType = it->second;
            }
            else {
                DIDEVCAPS caps = { sizeof(caps) };
                if (SUCCEEDED(pDevice->GetCapabilities(&caps))) {
                    devType = LOBYTE(caps.dwDevType);
                    g_device_types[pDevice] = devType;

                    // Log only on first discovery
                    if (g_logged_devices.find(pDevice) == g_logged_devices.end()) {
                        LOG_INFO("DI8: Device discovered: %p (Type: %d)", pDevice, devType);
                        g_logged_devices.insert(pDevice);
                    }
                }
            }
        }

        if (devType != 0)
        {
            // 1. Block Controllers (Always, if bBlockInput is true)
            if (BaseHook::Data::bBlockInput)
            {
                if (devType == DI8DEVTYPE_GAMEPAD || devType == DI8DEVTYPE_JOYSTICK ||
                    devType == DI8DEVTYPE_DRIVING || devType == DI8DEVTYPE_FLIGHT ||
                    devType == DI8DEVTYPE_1STPERSON || devType == DI8DEVTYPE_SUPPLEMENTAL)
                {
                    std::memset(lpvData, 0, cbData);

                    // FIX: In DirectInput, 0 on the POV Hat means "Up".
                    // We must set POV values to 0xFFFFFFFF (-1) to indicate "Centered".
                    if (cbData >= sizeof(DIJOYSTATE))
                    {
                        auto* js = static_cast<DIJOYSTATE*>(lpvData);
                        js->rgdwPOV[0] = 0xFFFFFFFF;
                        js->rgdwPOV[1] = 0xFFFFFFFF;
                        js->rgdwPOV[2] = 0xFFFFFFFF;
                        js->rgdwPOV[3] = 0xFFFFFFFF;
                    }
                }
            }

            // 2. Handle Initialization Phase (Block M/K only here, returns early)
            if (!BaseHook::Data::bIsInitialized)
            {
                if (BaseHook::Data::bBlockInput &&
                    (devType == DI8DEVTYPE_MOUSE || devType == DI8DEVTYPE_KEYBOARD))
                {
                    std::memset(lpvData, 0, cbData);
                }
                return hr;
            }

            // 3. Handle Mouse Data (ImGui)
            if (devType == DI8DEVTYPE_MOUSE && (cbData == sizeof(DIMOUSESTATE) || cbData == sizeof(DIMOUSESTATE2)))
            {
                auto* mouse_state = static_cast<DIMOUSESTATE*>(lpvData);
                ImGuiIO& io = ImGui::GetIO();

                io.MouseDelta.x += (float)mouse_state->lX;
                io.MouseDelta.y += (float)mouse_state->lY;
                io.MouseDown[0] = (mouse_state->rgbButtons[0] & 0x80) != 0;
                io.MouseDown[1] = (mouse_state->rgbButtons[1] & 0x80) != 0;
                io.MouseDown[2] = (mouse_state->rgbButtons[2] & 0x80) != 0;
                io.MouseDown[3] = (mouse_state->rgbButtons[3] & 0x80) != 0;
                if (cbData == sizeof(DIMOUSESTATE2))
                    io.MouseDown[4] = (mouse_state->rgbButtons[4] & 0x80) != 0;
                if (mouse_state->lZ != 0)
                {
                    io.MouseWheel += (float)mouse_state->lZ / WHEEL_DELTA;
                }

                if (BaseHook::Data::bBlockInput)
                {
                    std::memset(lpvData, 0, cbData);
                }
            }
            // 4. Handle Keyboard Blocking
            else if (devType == DI8DEVTYPE_KEYBOARD && BaseHook::Data::bBlockInput)
            {
                std::memset(lpvData, 0, cbData);
            }
        }
    }
    return hr;
}

HRESULT STDMETHODCALLTYPE hkGetDeviceData(IDirectInputDevice8* pDevice, DWORD cbObjectData, LPDIDEVICEOBJECTDATA rgdod, LPDWORD pdwInOut, DWORD dwFlags)
{
    HRESULT hr = oGetDeviceData(pDevice, cbObjectData, rgdod, pdwInOut, dwFlags);
    if (SUCCEEDED(hr) && BaseHook::Data::bBlockInput && (dwFlags & DIGDD_PEEK) == 0 && (rgdod != nullptr && pdwInOut && *pdwInOut != 0))
    {
        BYTE devType = 0;
        {
            std::lock_guard<std::mutex> lock(g_dinput_mutex);
            if (g_device_types.find(pDevice) != g_device_types.end())
                devType = g_device_types[pDevice];
        }

        // Fallback if not in cache yet (rare for GetDeviceData to be called before GetDeviceState, but possible)
        if (devType == 0) {
             DIDEVCAPS caps = { sizeof(caps) };
             if (SUCCEEDED(pDevice->GetCapabilities(&caps))) {
                 devType = LOBYTE(caps.dwDevType);
                 // Cache the result to prevent future slow calls
                 std::lock_guard<std::mutex> lock(g_dinput_mutex);
                 g_device_types[pDevice] = devType;
             }
        }

        if (devType != 0) {
            if (devType == DI8DEVTYPE_MOUSE || devType == DI8DEVTYPE_KEYBOARD ||
                devType == DI8DEVTYPE_GAMEPAD || devType == DI8DEVTYPE_JOYSTICK ||
                devType == DI8DEVTYPE_DRIVING || devType == DI8DEVTYPE_FLIGHT ||
                devType == DI8DEVTYPE_1STPERSON || devType == DI8DEVTYPE_SUPPLEMENTAL)
            {
                *pdwInOut = 0;
                hr = DI_OK; // Overwrite potential DI_BUFFEROVERFLOW
            }
        }
    }
    return hr;
}

// --- Hook for CreateDevice ---
HRESULT STDMETHODCALLTYPE hkCreateDevice(IDirectInput8* pDI, REFGUID rguid, LPDIRECTINPUTDEVICE8* lplpDirectInputDevice, LPUNKNOWN pUnkOuter)
{
    std::string guid_str = GuidToString(rguid);
    if (rguid == GUID_SysKeyboard) guid_str = "GUID_SysKeyboard";
    else if (rguid == GUID_SysMouse) guid_str = "GUID_SysMouse";

    LOG_INFO("DI8: CreateDevice called for GUID: %s.", guid_str.c_str());

    HRESULT hr = oCreateDevice(pDI, rguid, lplpDirectInputDevice, pUnkOuter);
    if (SUCCEEDED(hr) && lplpDirectInputDevice && *lplpDirectInputDevice)
    {
        void** vtable = *reinterpret_cast<void***>(*lplpDirectInputDevice);

        // Protect g_hooked_functions against concurrent device creation
        std::lock_guard<std::mutex> lock(g_dinput_mutex);

        void* pGetDeviceState = vtable[9];
        if (g_hooked_functions.find(pGetDeviceState) == g_hooked_functions.end())
        {
            if (MH_CreateHook(pGetDeviceState, &hkGetDeviceState, reinterpret_cast<void**>(&oGetDeviceState)) == MH_OK &&
                MH_EnableHook(pGetDeviceState) == MH_OK) {
                g_hooked_functions.insert(pGetDeviceState);
                LOG_INFO("DI8: Hooked GetDeviceState at %p", pGetDeviceState);
            } else {
                LOG_ERROR("DI8: Failed to hook GetDeviceState");
            }
        }

        void* pGetDeviceData = vtable[10];
        if (g_hooked_functions.find(pGetDeviceData) == g_hooked_functions.end())
        {
            if (MH_CreateHook(pGetDeviceData, &hkGetDeviceData, reinterpret_cast<void**>(&oGetDeviceData)) == MH_OK &&
                MH_EnableHook(pGetDeviceData) == MH_OK) {
                g_hooked_functions.insert(pGetDeviceData);
                LOG_INFO("DI8: Hooked GetDeviceData at %p", pGetDeviceData);
            } else {
                LOG_ERROR("DI8: Failed to hook GetDeviceData");
            }
        }
    }
    else {
        if (hr != DIERR_DEVICENOTREG && hr != E_FAIL)
            LOG_INFO("DI8: CreateDevice failed for GUID %s with HRESULT: 0x%X", guid_str.c_str(), hr);
    }
    return hr;
}


// --- Hook for DirectInput8Create ---
HRESULT WINAPI hkDirectInput8Create(HINSTANCE hinst, DWORD dwVersion, REFIID riidltf, LPVOID* ppvOut, LPUNKNOWN pUnkOuter)
{
    LOG_INFO("DI8: DirectInput8Create called.");
    HRESULT hr = oDirectInput8Create(hinst, dwVersion, riidltf, ppvOut, pUnkOuter);
    if (SUCCEEDED(hr) && ppvOut && *ppvOut)
    {
        IDirectInput8* pDI = static_cast<IDirectInput8*>(*ppvOut);
        void** vtable = *reinterpret_cast<void***>(pDI);
        void* pCreateDevice = vtable[3]; // IDirectInput8::CreateDevice is at index 3

        // Protect g_hooked_functions
        std::lock_guard<std::mutex> lock(g_dinput_mutex);

        if (g_hooked_functions.find(pCreateDevice) == g_hooked_functions.end())
        {
            if (MH_CreateHook(pCreateDevice, &hkCreateDevice, reinterpret_cast<void**>(&oCreateDevice)) == MH_OK &&
                MH_EnableHook(pCreateDevice) == MH_OK) {
                g_hooked_functions.insert(pCreateDevice);
                LOG_INFO("DI8: Hooked IDirectInput8::CreateDevice at %p", pCreateDevice);
            } else {
                LOG_ERROR("DI8: Failed to hook IDirectInput8::CreateDevice");
            }
        }
    }
    else {
        LOG_ERROR("DI8: Original DirectInput8Create failed with HRESULT: 0x%X", hr);
    }
    return hr;
}

namespace BaseHook {
namespace Hooks {
    void InitDirectInput()
    {
        LOG_INFO("DI8: Initializing DirectInput hooks...");
        HMODULE hDInput8 = GetModuleHandleA("dinput8.dll");
        if (!hDInput8) {
            hDInput8 = LoadLibraryA("dinput8.dll");
            if (!hDInput8) {
                LOG_ERROR("DI8: dinput8.dll not loaded and LoadLibrary failed. Skipping DirectInput hooks.");
                return;
            }
            LOG_INFO("DI8: Loaded dinput8.dll manually.");
        }

        void* pDirectInput8Create_target = GetProcAddress(hDInput8, "DirectInput8Create");
        if (!pDirectInput8Create_target)
        {
            LOG_ERROR("DI8: Could not find DirectInput8Create export.");
            return;
        }

        // --- Method 1: Dummy Device Hooking (catches existing devices) ---
        LOG_INFO("DI8: Attempting dummy device creation for VTable resolution...");

        using DirectInput8Create_t = HRESULT(WINAPI*)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);
        auto fnDirectInput8Create = (DirectInput8Create_t)pDirectInput8Create_target;

        IDirectInput8* pDummyDI = nullptr;
        HRESULT hr = fnDirectInput8Create(GetModuleHandle(NULL), 0x0800, IID_IDirectInput8, (void**)&pDummyDI, NULL);

        if (SUCCEEDED(hr) && pDummyDI)
        {
            IDirectInputDevice8* pDummyDevice = nullptr;
            // Create a dummy Mouse device to get the VTable
            hr = pDummyDI->CreateDevice(GUID_SysMouse, &pDummyDevice, NULL);
            if (SUCCEEDED(hr) && pDummyDevice)
            {
                void** vtable = *(void***)pDummyDevice;
                // Index 9: GetDeviceState
                // Index 10: GetDeviceData
                void* pGetDeviceState = vtable[9];
                void* pGetDeviceData = vtable[10];

                LOG_INFO("DI8: Dummy Device - GetDeviceState at %p", pGetDeviceState);
                LOG_INFO("DI8: Dummy Device - GetDeviceData at %p", pGetDeviceData);

                {
                    std::lock_guard<std::mutex> lock(g_dinput_mutex);

                    if (g_hooked_functions.find(pGetDeviceState) == g_hooked_functions.end())
                    {
                        if (MH_CreateHook(pGetDeviceState, &hkGetDeviceState, reinterpret_cast<void**>(&oGetDeviceState)) == MH_OK &&
                            MH_EnableHook(pGetDeviceState) == MH_OK) {
                            g_hooked_functions.insert(pGetDeviceState);
                            LOG_INFO("DI8: Hooked GetDeviceState via dummy device.");
                        } else {
                            LOG_ERROR("DI8: Failed to hook GetDeviceState via dummy.");
                        }
                    }

                    if (g_hooked_functions.find(pGetDeviceData) == g_hooked_functions.end())
                    {
                        if (MH_CreateHook(pGetDeviceData, &hkGetDeviceData, reinterpret_cast<void**>(&oGetDeviceData)) == MH_OK &&
                            MH_EnableHook(pGetDeviceData) == MH_OK) {
                            g_hooked_functions.insert(pGetDeviceData);
                            LOG_INFO("DI8: Hooked GetDeviceData via dummy device.");
                        } else {
                            LOG_ERROR("DI8: Failed to hook GetDeviceData via dummy.");
                        }
                    }
                }

                pDummyDevice->Release();
            }
            else
            {
                LOG_ERROR("DI8: Failed to create dummy mouse device. HR: 0x%X", hr);
            }
            pDummyDI->Release();
        }
        else
        {
            LOG_ERROR("DI8: Failed to create dummy DirectInput8 interface. HR: 0x%X", hr);
        }

        // --- Method 2: Export Hooking (catches future creation) ---
        {
            std::lock_guard<std::mutex> lock(g_dinput_mutex);

            if (g_hooked_functions.find(pDirectInput8Create_target) == g_hooked_functions.end())
            {
                if (MH_CreateHook(pDirectInput8Create_target, &hkDirectInput8Create, reinterpret_cast<void**>(&oDirectInput8Create)) == MH_OK &&
                    MH_EnableHook(pDirectInput8Create_target) == MH_OK) {
                    g_hooked_functions.insert(pDirectInput8Create_target);
                    LOG_INFO("DI8: Hooked DirectInput8Create export at %p", pDirectInput8Create_target);
                } else {
                    LOG_ERROR("DI8: Failed to hook DirectInput8Create");
                }
            }
        }
    }

}
}
