#include "pch.h"
#include "core/BaseHook.h"
#include "hooks/InputHooks.h"
#include "hooks/WindowHooks.h"
#include <unordered_set>
#include <unordered_map>
#include <shared_mutex>
#include <mutex>
#include <algorithm>
#include <vector>
#include <memory>
#include <string>
#include <cwctype>
#include <chrono>
#include <atomic>
#include <cmath>
#include <thread>
#include <Dbt.h>
#include <xinput.h>
#include <hidclass.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <setupapi.h>

#pragma comment(lib, "hid.lib")
#pragma comment(lib, "setupapi.lib")

#ifndef XINPUT_GAMEPAD_GUIDE
#define XINPUT_GAMEPAD_GUIDE 0x0400
#endif

using BaseHook::Hooks::GamepadInputSource;

namespace BaseHook
{
    namespace Hooks
    {

static std::wstring ToWideString(const TCHAR* path)
{
    if (!path)
        return {};
#ifdef UNICODE
    return std::wstring(path);
#else
    int len = MultiByteToWideChar(CP_ACP, 0, path, -1, nullptr, 0);
    if (len <= 1)
        return {};
    std::wstring wide(len - 1, L'\0');
    MultiByteToWideChar(CP_ACP, 0, path, -1, wide.data(), len);
    return wide;
#endif
}

// --- Function Typedefs ---
typedef HRESULT(WINAPI* DirectInput8Create_t)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);
typedef HRESULT(STDMETHODCALLTYPE* IDirectInput8_CreateDevice_t)(IDirectInput8*, REFGUID, LPDIRECTINPUTDEVICE8*, LPUNKNOWN);
typedef HRESULT(STDMETHODCALLTYPE* IDirectInputDevice8_GetDeviceState_t)(IDirectInputDevice8*, DWORD, LPVOID);
typedef HRESULT(STDMETHODCALLTYPE* IDirectInputDevice8_GetDeviceData_t)(IDirectInputDevice8*, DWORD, LPDIDEVICEOBJECTDATA, LPDWORD, DWORD);
typedef HRESULT(STDMETHODCALLTYPE* IDirectInputDevice8_SetCooperativeLevel_t)(IDirectInputDevice8*, HWND, DWORD);

// IIDs for A and W interfaces (Defined locally to avoid SDK dependencies)
static const GUID IID_IDirectInput8A_Local = { 0xBF798030, 0x483A, 0x4DA2, { 0xAA, 0x99, 0x5D, 0x64, 0xED, 0x36, 0x97, 0x00 } };
static const GUID IID_IDirectInput8W_Local = { 0xBF798031, 0x483A, 0x4DA2, { 0xAA, 0x99, 0x5D, 0x64, 0xED, 0x36, 0x97, 0x00 } };

// --- Original Functions ---
static DirectInput8Create_t oDirectInput8Create = nullptr;

// Separate trampolines for ANSI and Unicode to prevent collisions
// Map for IDirectInput8 methods (Address -> Trampoline)
static std::unordered_map<void*, void*> g_FactoryMethodTrampolines;

// Shared trampolines for Device methods (VTable is shared or identical signature)
// Trampoline Map for Device methods (Address -> Trampoline)
static std::unordered_map<void*, void*> g_DeviceMethodTrampolines;

// Trackers
static std::unordered_set<void*> g_hooked_functions;

struct AxisRange {
    LONG min = 0;
    LONG max = 65535;

    LONG GetCenter() const { return (min + max) / 2; }

    float Normalize(LONG val) const {
        if (min >= max) return 0.0f;
        float center = (float)(max + min) * 0.5f;
        float range = (float)(max - min) * 0.5f; // Half range
        float norm = ((float)val - center) / range;

        // Clamp
        if (norm < -1.0f) norm = -1.0f;
        if (norm > 1.0f) norm = 1.0f;

        return norm;
    }
};

// Named constants for axis indices in DeviceInfo::axes array
enum AxisIndex {
    AXIS_X = 0,    // Left stick X
    AXIS_Y = 1,    // Left stick Y  
    AXIS_Z = 2,    // Right stick X (or Z-axis)
    AXIS_RX = 3,   // Right stick X rotation
    AXIS_RY = 4,   // Right stick Y rotation
    AXIS_RZ = 5    // Right stick Y (or Z-rotation)
};

struct DeviceInfo {
    BYTE type = 0;
    DWORD vid = 0;
    DWORD pid = 0;
    AxisRange axes[6]; // Indexed by AxisIndex enum
};

static std::unordered_map<IDirectInputDevice8*, DeviceInfo> g_Devices;
static std::unordered_set<IDirectInputDevice8*> g_FailedDevices;
static std::shared_mutex g_dinput_mutex;
static IDirectInputDevice8* g_PrimaryDevice = nullptr; 
static std::atomic<unsigned long long> g_lastAuthoritativeUpdateMs{ 0 };
constexpr DWORD kPrimaryFreshThresholdMs = 250; 
static thread_local bool g_isPrivatePolling = false;

// --- Helper: Apply HID (Virtual XInput) State to DirectInput Buffer ---
static void ApplyHIDStateToDI(const XINPUT_STATE& xState, const DeviceInfo& info, LPVOID lpvData, DWORD cbData)
{
    if (!lpvData || cbData < sizeof(DIJOYSTATE))
        return;

    // Cast to DIJOYSTATE2 (superset of DIJOYSTATE)
    // We clear the buffer first to ensure clean state
    std::memset(lpvData, 0, cbData);

    auto* js = static_cast<DIJOYSTATE2*>(lpvData);
    const XINPUT_GAMEPAD& pad = xState.Gamepad;

    // --- Axes ---
    // Map XInput (-32768 to 32767) to DirectInput Axis Ranges
    auto mapAxis = [](SHORT val, const AxisRange& range, bool invert) -> LONG {
        float norm = (val < 0) ? (float)val / 32768.0f : (float)val / 32767.0f;
        if (invert) norm = -norm;
        // Map -1..1 to range.min..range.max
        float center = (range.min + range.max) * 0.5f;
        float halfRange = (range.max - range.min) * 0.5f;
        return static_cast<LONG>(center + (norm * halfRange));
    };

    js->lX = mapAxis(pad.sThumbLX, info.axes[AXIS_X], false);
    js->lY = mapAxis(pad.sThumbLY, info.axes[AXIS_Y], true); // Invert Y for DInput
    js->lZ = mapAxis(pad.sThumbRX, info.axes[AXIS_Z], false);

    // Map Right Stick Y to Rz (Axis 5)
    if (cbData >= sizeof(DIJOYSTATE))
        js->lRz = mapAxis(pad.sThumbRY, info.axes[AXIS_RZ], true);

    // --- POV ---
    DWORD pov = (DWORD)-1;
    if (pad.wButtons & XINPUT_GAMEPAD_DPAD_UP) {
        if (pad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) pov = 4500;
        else if (pad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) pov = 31500;
        else pov = 0;
    } else if (pad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) {
        if (pad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) pov = 13500;
        else if (pad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) pov = 22500;
        else pov = 18000;
    } else if (pad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) {
        pov = 9000;
    } else if (pad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) {
        pov = 27000;
    }
    js->rgdwPOV[0] = pov;

    // --- Buttons ---
    auto setBtn = [&](int idx, bool pressed) { if (idx < 128) js->rgbButtons[idx] = pressed ? 0x80 : 0x00; };

    setBtn(0, (pad.wButtons & XINPUT_GAMEPAD_X) != 0);
    setBtn(1, (pad.wButtons & XINPUT_GAMEPAD_A) != 0);
    setBtn(2, (pad.wButtons & XINPUT_GAMEPAD_B) != 0);
    setBtn(3, (pad.wButtons & XINPUT_GAMEPAD_Y) != 0);

    setBtn(4, (pad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0);  // L1
    setBtn(5, (pad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0); // R1

    setBtn(6, pad.bLeftTrigger > 30);  // L2
    setBtn(7, pad.bRightTrigger > 30); // R2

    setBtn(8, (pad.wButtons & XINPUT_GAMEPAD_BACK) != 0);  // Share/Select
    setBtn(9, (pad.wButtons & XINPUT_GAMEPAD_START) != 0); // Options/Start
    setBtn(10, (pad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) != 0); // L3
    setBtn(11, (pad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) != 0); // R3
    setBtn(12, (pad.wButtons & XINPUT_GAMEPAD_GUIDE) != 0); // PS
}

// Thread-safe Input Buffer
static std::mutex g_input_queue_mutex;
struct MouseBuffer {
    long lX = 0;
    long lY = 0;
    long lZ = 0;
    BYTE rgbButtons[8] = {0};
} g_mouse_buffer;

struct VirtualPadState
{
    XINPUT_STATE state{};
    GamepadInputSource source = GamepadInputSource::None;
    void* context = nullptr;
    bool hasData = false;
    ULONGLONG lastUpdateMs = 0;
    DWORD packetCounter = 1;
};

static VirtualPadState g_virtualPad;
static std::mutex g_virtualPadMutex;
constexpr ULONGLONG kVirtualStateStaleMs = 500;

static int GetSourcePriority(GamepadInputSource source)
{
    switch (source)
    {
    case GamepadInputSource::SonyHID:         return 3;
    case GamepadInputSource::HookedDevice:    return 2;
    case GamepadInputSource::PrivateFallback: return 1;
    default:                                  return 0;
    }
}

static bool UpdateVirtualPad(GamepadInputSource source, void* context, const XINPUT_STATE& translated, bool& outSourceChanged)
{
    outSourceChanged = false;

    std::lock_guard<std::mutex> lock(g_virtualPadMutex);
    const ULONGLONG now = GetTickCount64();
    const bool sameContext = g_virtualPad.hasData && g_virtualPad.source == source && g_virtualPad.context == context;
    const bool stale = !g_virtualPad.hasData || (now - g_virtualPad.lastUpdateMs) > kVirtualStateStaleMs;

    if (g_virtualPad.hasData && !sameContext && !stale)
    {
        if (GetSourcePriority(source) < GetSourcePriority(g_virtualPad.source))
            return false;
    }

    const bool swappedSource = !sameContext;

    g_virtualPad.state = translated;
    g_virtualPad.state.dwPacketNumber = ++g_virtualPad.packetCounter;
    g_virtualPad.source = source;
    g_virtualPad.context = context;
    g_virtualPad.hasData = true;
    g_virtualPad.lastUpdateMs = now;

    outSourceChanged = swappedSource;
    return true;
}

static void ResetVirtualPad(GamepadInputSource sourceFilter = GamepadInputSource::None, void* contextFilter = nullptr)
{
    std::lock_guard<std::mutex> lock(g_virtualPadMutex);
    if (!g_virtualPad.hasData)
        return;

    if (sourceFilter != GamepadInputSource::None && g_virtualPad.source != sourceFilter)
        return;

    if (contextFilter && g_virtualPad.context != contextFilter)
        return;

    g_virtualPad = {};
}

static SHORT AxisToThumbValue(const AxisRange& range, LONG value, bool invert = false)
{
    float normalized = range.Normalize(value);
    if (invert)
        normalized = -normalized;

    float scaled = normalized * 32767.0f;
    if (scaled > 32767.0f) scaled = 32767.0f;
    if (scaled < -32767.0f) scaled = -32767.0f;
    return static_cast<SHORT>(scaled);
}

static BYTE DigitalTriggerValue(bool pressed)
{
    return pressed ? 255 : 0;
}

static void ApplyPOVToButtons(DWORD pov, WORD& buttons)
{
    if (pov == static_cast<DWORD>(-1))
        return;

    if (pov > 27000 || pov < 9000)
        buttons |= XINPUT_GAMEPAD_DPAD_UP;
    if (pov > 9000 && pov < 27000)
        buttons |= XINPUT_GAMEPAD_DPAD_DOWN;
    if (pov > 18000)
        buttons |= XINPUT_GAMEPAD_DPAD_LEFT;
    if (pov > 0 && pov < 18000)
        buttons |= XINPUT_GAMEPAD_DPAD_RIGHT;
}

static XINPUT_STATE BuildVirtualXInputState(const DIJOYSTATE2& state, const DeviceInfo& info)
{
    XINPUT_STATE xState{};
    XINPUT_GAMEPAD& pad = xState.Gamepad;

    auto buttonPressed = [&](int index) -> bool
    {
        if (index < 0 || index >= 128)
            return false;
        return (state.rgbButtons[index] & 0x80) != 0;
    };

    // Face buttons (PS layout -> XInput)
    if (buttonPressed(1)) pad.wButtons |= XINPUT_GAMEPAD_A; // Cross
    if (buttonPressed(2)) pad.wButtons |= XINPUT_GAMEPAD_B; // Circle
    if (buttonPressed(3)) pad.wButtons |= XINPUT_GAMEPAD_Y; // Triangle
    if (buttonPressed(0)) pad.wButtons |= XINPUT_GAMEPAD_X; // Square

    // Shoulders
    if (buttonPressed(4)) pad.wButtons |= XINPUT_GAMEPAD_LEFT_SHOULDER;
    if (buttonPressed(5)) pad.wButtons |= XINPUT_GAMEPAD_RIGHT_SHOULDER;

    // Center buttons
    if (buttonPressed(8)) pad.wButtons |= XINPUT_GAMEPAD_BACK;
    if (buttonPressed(9)) pad.wButtons |= XINPUT_GAMEPAD_START;

    // Stick presses
    if (buttonPressed(10)) pad.wButtons |= XINPUT_GAMEPAD_LEFT_THUMB;
    if (buttonPressed(11)) pad.wButtons |= XINPUT_GAMEPAD_RIGHT_THUMB;

    // POV / Dpad
    ApplyPOVToButtons(state.rgdwPOV[0], pad.wButtons);

    // Triggers (digital fallbacks)
    pad.bLeftTrigger = DigitalTriggerValue(buttonPressed(6));
    pad.bRightTrigger = DigitalTriggerValue(buttonPressed(7));

    // Sticks
    pad.sThumbLX = AxisToThumbValue(info.axes[AXIS_X], state.lX);
    pad.sThumbLY = AxisToThumbValue(info.axes[AXIS_Y], state.lY, true);
    pad.sThumbRX = AxisToThumbValue(info.axes[AXIS_Z], state.lZ);
    pad.sThumbRY = AxisToThumbValue(info.axes[AXIS_RZ], state.lRz, true);

    return xState;
}

static bool TryCopyVirtualPad(XINPUT_STATE* outState)
{
    if (!outState)
        return false;

    std::lock_guard<std::mutex> lock(g_virtualPadMutex);
    if (!g_virtualPad.hasData)
        return false;

    ULONGLONG elapsed = GetTickCount64() - g_virtualPad.lastUpdateMs;
    if (elapsed > kVirtualStateStaleMs)
        return false;

    *outState = g_virtualPad.state;
    return true;
}

static void ApplyVirtualPadToImGui()
{
    XINPUT_STATE state{};
    if (!TryCopyVirtualPad(&state))
        return;

    ImGuiIO& io = ImGui::GetIO();
    io.BackendFlags |= ImGuiBackendFlags_HasGamepad;

    auto pressed = [&](WORD mask) -> bool { return (state.Gamepad.wButtons & mask) != 0; };
    auto stickValue = [](SHORT v) -> float {
        float value = (v >= 0) ? (static_cast<float>(v) / 32767.0f) : (static_cast<float>(v) / 32768.0f);
        if (value < -1.0f) value = -1.0f;
        if (value > 1.0f) value = 1.0f;
        return value;
    };
    auto addStick = [&](ImGuiKey negative, ImGuiKey positive, float value) {
        const float threshold = 0.1f;
        io.AddKeyAnalogEvent(negative, value < -threshold, value < -threshold ? -value : 0.0f);
        io.AddKeyAnalogEvent(positive, value > threshold, value > threshold ? value : 0.0f);
    };

    // Face buttons
    io.AddKeyEvent(ImGuiKey_GamepadFaceDown, pressed(XINPUT_GAMEPAD_A));
    io.AddKeyEvent(ImGuiKey_GamepadFaceRight, pressed(XINPUT_GAMEPAD_B));
    io.AddKeyEvent(ImGuiKey_GamepadFaceUp, pressed(XINPUT_GAMEPAD_Y));
    io.AddKeyEvent(ImGuiKey_GamepadFaceLeft, pressed(XINPUT_GAMEPAD_X));

    // Shoulders & center
    io.AddKeyEvent(ImGuiKey_GamepadL1, pressed(XINPUT_GAMEPAD_LEFT_SHOULDER));
    io.AddKeyEvent(ImGuiKey_GamepadR1, pressed(XINPUT_GAMEPAD_RIGHT_SHOULDER));
    io.AddKeyEvent(ImGuiKey_GamepadBack, pressed(XINPUT_GAMEPAD_BACK));
    io.AddKeyEvent(ImGuiKey_GamepadStart, pressed(XINPUT_GAMEPAD_START));
    io.AddKeyEvent(ImGuiKey_GamepadL3, pressed(XINPUT_GAMEPAD_LEFT_THUMB));
    io.AddKeyEvent(ImGuiKey_GamepadR3, pressed(XINPUT_GAMEPAD_RIGHT_THUMB));

    // Dpad
    io.AddKeyEvent(ImGuiKey_GamepadDpadUp, pressed(XINPUT_GAMEPAD_DPAD_UP));
    io.AddKeyEvent(ImGuiKey_GamepadDpadDown, pressed(XINPUT_GAMEPAD_DPAD_DOWN));
    io.AddKeyEvent(ImGuiKey_GamepadDpadLeft, pressed(XINPUT_GAMEPAD_DPAD_LEFT));
    io.AddKeyEvent(ImGuiKey_GamepadDpadRight, pressed(XINPUT_GAMEPAD_DPAD_RIGHT));

    // Triggers
    const float lTrigger = state.Gamepad.bLeftTrigger / 255.0f;
    const float rTrigger = state.Gamepad.bRightTrigger / 255.0f;
    io.AddKeyAnalogEvent(ImGuiKey_GamepadL2, lTrigger > 0.05f, lTrigger);
    io.AddKeyAnalogEvent(ImGuiKey_GamepadR2, rTrigger > 0.05f, rTrigger);

    // Sticks
    const float lx = stickValue(state.Gamepad.sThumbLX);
    const float ly = stickValue(state.Gamepad.sThumbLY);
    const float rx = stickValue(state.Gamepad.sThumbRX);
    const float ry = stickValue(state.Gamepad.sThumbRY);

    addStick(ImGuiKey_GamepadLStickLeft, ImGuiKey_GamepadLStickRight, lx);
    addStick(ImGuiKey_GamepadLStickUp, ImGuiKey_GamepadLStickDown, -ly);
    addStick(ImGuiKey_GamepadRStickLeft, ImGuiKey_GamepadRStickRight, rx);
    addStick(ImGuiKey_GamepadRStickUp, ImGuiKey_GamepadRStickDown, -ry);
}

namespace
{
    struct PrivateDevice
    {
        GUID guidInstance{};
        IDirectInputDevice8* device = nullptr;
        DeviceInfo info{};
        bool seenInLastEnum = false;
    };

    std::mutex g_privateDeviceMutex;
    IDirectInput8* g_privateDI = nullptr;
    std::vector<PrivateDevice> g_privateDevices;
    HWND g_privateWindow = nullptr;
    std::atomic<bool> g_pendingPrivateRefresh = false;
    std::atomic<bool> g_isRefreshing = false;
    std::chrono::steady_clock::time_point g_lastPrivateEnum{};
    std::chrono::steady_clock::time_point g_lastPrivateSampleLog{};
    constexpr std::chrono::seconds kPrivateRescanInterval(2);

    // --- XInput Detection Helper ---
    bool IsXInputDevice(DWORD vid, DWORD pid)
    {
        UINT nDevices = 0;
        if (GetRawInputDeviceList(NULL, &nDevices, sizeof(RAWINPUTDEVICELIST)) != 0) return false;
        if (nDevices == 0) return false;

        std::vector<RAWINPUTDEVICELIST> devices(nDevices);
        if (GetRawInputDeviceList(devices.data(), &nDevices, sizeof(RAWINPUTDEVICELIST)) == (UINT)-1) return false;

        for (const auto& device : devices)
        {
            if (device.dwType != RIM_TYPEHID) continue;

            UINT size = 0;
            GetRawInputDeviceInfoA(device.hDevice, RIDI_DEVICENAME, NULL, &size);
            if (size == 0) continue;

            std::string name(size, '\0');
            if (GetRawInputDeviceInfoA(device.hDevice, RIDI_DEVICENAME, &name[0], &size) <= 0) continue;

            std::transform(name.begin(), name.end(), name.begin(), ::tolower);

            // XInput devices contain "ig_" in the device path
            if (name.find("ig_") == std::string::npos) continue;

            char vidStr[16], pidStr[16];
            snprintf(vidStr, sizeof(vidStr), "vid_%04x", vid);
            snprintf(pidStr, sizeof(pidStr), "pid_%04x", pid);

            if (name.find(vidStr) != std::string::npos && name.find(pidStr) != std::string::npos)
                return true;
        }
        return false;
    }

    BOOL CALLBACK EnumGamepadsCallback(const DIDEVICEINSTANCE* instance, VOID* context)
    {
        if (!instance || !context) return DIENUM_STOP;
        auto* guids = reinterpret_cast<std::vector<GUID>*>(context);
        guids->push_back(instance->guidInstance);
        return DIENUM_CONTINUE;
    }

    void ReleasePrivateDevice(PrivateDevice& dev)
    {
        if (dev.device)
        {
            ResetVirtualPad(GamepadInputSource::PrivateFallback, dev.device);
            dev.device->Unacquire();
            dev.device->Release();
            dev.device = nullptr;
        }
    }

    void PopulateAxisInfo(IDirectInputDevice8* device, DeviceInfo& info)
    {
        if (!device) return;
        DIDEVCAPS caps = { sizeof(caps) };
        if (FAILED(device->GetCapabilities(&caps))) return;

        DIPROPDWORD dipdw;
        dipdw.diph.dwSize = sizeof(DIPROPDWORD);
        dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
        dipdw.diph.dwObj = 0;
        dipdw.diph.dwHow = DIPH_DEVICE;
        if (SUCCEEDED(device->GetProperty(DIPROP_VIDPID, &dipdw.diph))) {
            info.vid = LOWORD(dipdw.dwData);
            info.pid = HIWORD(dipdw.dwData);
        }

        info.type = LOBYTE(caps.dwDevType);
        if (info.type == DI8DEVTYPE_MOUSE || info.type == DI8DEVTYPE_KEYBOARD)
            return;

        const DWORD axisOffsets[] = { DIJOFS_X, DIJOFS_Y, DIJOFS_Z, DIJOFS_RX, DIJOFS_RY, DIJOFS_RZ };
        for (int i = 0; i < 6; i++)
        {
            DIPROPRANGE range{};
            range.diph.dwSize = sizeof(DIPROPRANGE);
            range.diph.dwHeaderSize = sizeof(DIPROPHEADER);
            range.diph.dwHow = DIPH_BYOFFSET;
            range.diph.dwObj = axisOffsets[i];

            if (SUCCEEDED(device->GetProperty(DIPROP_RANGE, &range.diph)))
            {
                info.axes[i].min = range.lMin;
                info.axes[i].max = range.lMax;
            }
        }
    }

    bool EnsurePrivateContext()
    {
        if (g_privateDI) return true;
        if (!oDirectInput8Create) return false;

        HRESULT hr = oDirectInput8Create(GetModuleHandle(NULL), DIRECTINPUT_VERSION, IID_IDirectInput8W, (void**)&g_privateDI, NULL);
        if (FAILED(hr))
        {
            LOG_ERROR("DI8: Failed to create private DirectInput interface. hr=0x%08X", hr);
            return false;
        }

        return true;
    }

    bool CreatePrivateDevice(const GUID& guid, PrivateDevice& outDevice)
    {
        if (!g_privateDI || !g_privateWindow)
            return false;

        IDirectInputDevice8* device = nullptr;
        HRESULT hr = g_privateDI->CreateDevice(guid, &device, nullptr);
        if (FAILED(hr) || !device)
            return false;

        // Check for XInput
        {
            DIPROPDWORD dipdw;
            dipdw.diph.dwSize = sizeof(DIPROPDWORD);
            dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
            dipdw.diph.dwObj = 0;
            dipdw.diph.dwHow = DIPH_DEVICE;
            
            if (SUCCEEDED(device->GetProperty(DIPROP_VIDPID, &dipdw.diph)))
            {
                DWORD vid = LOWORD(dipdw.dwData);
                DWORD pid = HIWORD(dipdw.dwData);
                
                if (IsXInputDevice(vid, pid))
                {
                    device->Release();
                    return false;
                }
            }
        }

        hr = device->SetDataFormat(&c_dfDIJoystick2);
        if (FAILED(hr))
        {
            device->Release();
            return false;
        }

        HWND coopWindow = g_privateWindow ? g_privateWindow : GetForegroundWindow();
        if (!coopWindow) coopWindow = GetDesktopWindow();

        hr = device->SetCooperativeLevel(coopWindow, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE);
        if (FAILED(hr))
        {
            device->Release();
            return false;
        }

        device->Acquire();

        outDevice.guidInstance = guid;
        outDevice.device = device;
        outDevice.seenInLastEnum = true;
        PopulateAxisInfo(device, outDevice.info);
        return true;
    }

    void RefreshPrivateDevices(bool force)
    {
        if (!g_privateWindow) return;
        if (!EnsurePrivateContext()) return;

        auto now = std::chrono::steady_clock::now();
        bool requested = g_pendingPrivateRefresh.exchange(false);
        
        if (!force)
        {
            if (!requested && g_lastPrivateEnum.time_since_epoch().count() != 0)
            {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_lastPrivateEnum);
                if (elapsed < kPrivateRescanInterval)
                    return;
            }
        }
        g_lastPrivateEnum = now;

        bool expected = false;
        if (!g_isRefreshing.compare_exchange_strong(expected, true))
            return;

        std::thread([=]() {
            std::vector<GUID> discovered;
            HRESULT hr = g_privateDI->EnumDevices(DI8DEVCLASS_GAMECTRL, EnumGamepadsCallback, &discovered, DIEDFL_ATTACHEDONLY);
            
            if (FAILED(hr)) {
                g_isRefreshing = false;
                return;
            }

            std::vector<GUID> existingGuids;
            {
                std::lock_guard<std::mutex> lock(g_privateDeviceMutex);
                for (const auto& dev : g_privateDevices)
                    existingGuids.push_back(dev.guidInstance);
            }

            std::vector<PrivateDevice> newDevices;
            for (const auto& guid : discovered)
            {
                bool exists = false;
                for (const auto& ex : existingGuids) {
                    if (ex == guid) { exists = true; break; }
                }

                if (!exists)
                {
                    PrivateDevice newDev{};
                    if (CreatePrivateDevice(guid, newDev))
                    {
                        newDevices.push_back(std::move(newDev));
                    }
                }
            }

            {
                std::lock_guard<std::mutex> lock(g_privateDeviceMutex);
                
                for (auto& dev : g_privateDevices)
                    dev.seenInLastEnum = false;

                for (const auto& guid : discovered)
                {
                    for (auto& dev : g_privateDevices) {
                        if (dev.guidInstance == guid) {
                            dev.seenInLastEnum = true;
                            break;
                        }
                    }
                }

                for (auto& newDev : newDevices)
                    g_privateDevices.push_back(std::move(newDev));

                auto it = g_privateDevices.begin();
                while (it != g_privateDevices.end())
                {
                    if (!it->seenInLastEnum)
                    {
                        ReleasePrivateDevice(*it);
                        it = g_privateDevices.erase(it);
                    }
                    else
                    {
                        ++it;
                    }
                }
            }

            g_isRefreshing = false;
        }).detach();
    }

    bool CommitPrivateState(const PrivateDevice& dev, const DIJOYSTATE2& state)
    {
        XINPUT_STATE translated = BuildVirtualXInputState(state, dev.info);
        bool swapped = false;
        bool accepted = BaseHook::Hooks::SubmitVirtualGamepadState(GamepadInputSource::PrivateFallback, dev.device, translated, false, &swapped);
        return accepted && swapped;
    }

    void PollPrivateDevicesFallback()
    {
        if (!g_privateWindow) return;

        const unsigned long long now = GetTickCount64();
        const unsigned long long lastPrimary = g_lastAuthoritativeUpdateMs.load(std::memory_order_relaxed);
        const bool primaryFresh = (lastPrimary != 0) && (now - lastPrimary) <= kPrimaryFreshThresholdMs;
        if (primaryFresh)
            return;

        RefreshPrivateDevices(false);

        std::lock_guard<std::mutex> lock(g_privateDeviceMutex);
        for (auto& dev : g_privateDevices)
        {
            if (!dev.device) continue;

            DIJOYSTATE2 state{};
            HRESULT hr = dev.device->Poll();
            if (FAILED(hr))
            {
                hr = dev.device->Acquire();
                if (FAILED(hr)) continue; 
                hr = dev.device->Poll();
                if (FAILED(hr)) continue;
            }

            struct PrivatePollScope
            {
                PrivatePollScope() { g_isPrivatePolling = true; }
                ~PrivatePollScope() { g_isPrivatePolling = false; }
            } pollScope;

            hr = dev.device->GetDeviceState(sizeof(state), &state);
            if (FAILED(hr))
            {
                if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED)
                    dev.device->Acquire();
                continue;
            }

            if (CommitPrivateState(dev, state))
            {
                LOG_INFO("DI8: Switched virtual XInput source to private DirectInput device %p.", dev.device);
            }
            break; 
        }
    }

    void SchedulePrivateRefresh()
    {
        g_pendingPrivateRefresh.store(true);
    }

    // --- Unified Sony HID Support ---
    constexpr USHORT kSonyVendorId = 0x054c;
    constexpr USHORT kDualSenseProductId = 0x0ce6;
    const std::vector<USHORT> kDualShock4ProductIds = { 0x05c4, 0x09cc };

    enum class SonyControllerType { Unknown, DualShock4, DualSense };

    struct SonyDevice
    {
        std::wstring path;
        HANDLE handle = INVALID_HANDLE_VALUE;
        HIDP_CAPS caps{};
        bool bluetooth = false;
        SonyControllerType type = SonyControllerType::Unknown;
        USHORT pid = 0;

        std::vector<uint8_t> inputBuffer;
        std::atomic<bool> running{ false };
        std::thread readerThread;
        bool seenInRefresh = false;

        ~SonyDevice() { Stop(); }

        void Stop()
        {
            if (running.exchange(false))
            {
                if (readerThread.joinable())
                {
                    CancelSynchronousIo(readerThread.native_handle());
                    readerThread.join();
                }
            }
            else if (readerThread.joinable())
            {
                readerThread.join();
            }

            if (handle != INVALID_HANDLE_VALUE)
            {
                CloseHandle(handle);
                handle = INVALID_HANDLE_VALUE;
            }
        }
    };

    std::mutex g_sonyDeviceMutex;
    std::vector<std::unique_ptr<SonyDevice>> g_sonyDevices;
    std::atomic<bool> g_sonyInitialized{ false };
    std::atomic<bool> g_sonyPendingRefresh{ false };
    std::atomic<unsigned long long> g_lastSonyRefreshMs{ 0 };
    constexpr unsigned long long kSonyAutoRefreshMs = 2000;

    bool HasSonyDevices()
    {
        std::lock_guard<std::mutex> lock(g_sonyDeviceMutex);
        return !g_sonyDevices.empty();
    }

    // Common Helpers
    SHORT SonyStickFromByte(uint8_t value, bool invert = false)
    {
        float normalized = (static_cast<float>(value) / 255.0f) * 2.0f - 1.0f;
        if (invert) normalized = -normalized;
        float scaled = normalized * 32767.0f;
        if (scaled > 32767.0f) scaled = 32767.0f;
        if (scaled < -32767.0f) scaled = -32767.0f;
        return static_cast<SHORT>(scaled);
    }

    void ApplySonyHat(uint8_t hat, WORD& buttons)
    {
        switch (hat)
        {
        case 0: buttons |= XINPUT_GAMEPAD_DPAD_UP; break;
        case 1: buttons |= XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_DPAD_RIGHT; break;
        case 2: buttons |= XINPUT_GAMEPAD_DPAD_RIGHT; break;
        case 3: buttons |= XINPUT_GAMEPAD_DPAD_RIGHT | XINPUT_GAMEPAD_DPAD_DOWN; break;
        case 4: buttons |= XINPUT_GAMEPAD_DPAD_DOWN; break;
        case 5: buttons |= XINPUT_GAMEPAD_DPAD_DOWN | XINPUT_GAMEPAD_DPAD_LEFT; break;
        case 6: buttons |= XINPUT_GAMEPAD_DPAD_LEFT; break;
        case 7: buttons |= XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_DPAD_UP; break;
        default: break;
        }
    }

    // --- DualSense Parsing ---
    struct DualSenseParsedState
    {
        uint8_t lx = 128, ly = 128, rx = 128, ry = 128;
        uint8_t l2 = 0, r2 = 0;
        uint8_t dpad = 8;
        bool square = false, cross = false, circle = false, triangle = false;
        bool l1 = false, r1 = false, l2Button = false, r2Button = false;
        bool create = false, options = false, l3 = false, r3 = false;
        bool ps = false, touchpad = false, mute = false;
    };

    bool ParseDualSenseUsbReport(const uint8_t* data, size_t len, DualSenseParsedState& parsed)
    {
        if (len < 64 || data[0] != 0x01) return false;
        const uint8_t* payload = data + 1;
        parsed.lx = payload[0]; parsed.ly = payload[1];
        parsed.rx = payload[2]; parsed.ry = payload[3];
        parsed.l2 = payload[4]; parsed.r2 = payload[5];

        uint8_t buttons0 = payload[7];
        uint8_t buttons1 = payload[8];
        uint8_t buttons2 = payload[9];

        parsed.dpad = buttons0 & 0x0F;
        parsed.square = (buttons0 & 0x10); parsed.cross = (buttons0 & 0x20);
        parsed.circle = (buttons0 & 0x40); parsed.triangle = (buttons0 & 0x80);

        parsed.l1 = (buttons1 & 0x01); parsed.r1 = (buttons1 & 0x02);
        parsed.l2Button = (buttons1 & 0x04); parsed.r2Button = (buttons1 & 0x08);
        parsed.create = (buttons1 & 0x10); parsed.options = (buttons1 & 0x20);
        parsed.l3 = (buttons1 & 0x40); parsed.r3 = (buttons1 & 0x80);

        parsed.ps = (buttons2 & 0x01); parsed.touchpad = (buttons2 & 0x02); parsed.mute = (buttons2 & 0x04);
        return true;
    }

    bool ParseDualSenseBtShortReport(const uint8_t* data, size_t len, DualSenseParsedState& parsed)
    {
        if (len < 10 || data[0] != 0x01) return false;
        const uint8_t* payload = data + 1;
        parsed.lx = payload[0]; parsed.ly = payload[1];
        parsed.rx = payload[2]; parsed.ry = payload[3];
        parsed.dpad = payload[4] & 0x0F;
        parsed.square = (payload[4] & 0x10); parsed.cross = (payload[4] & 0x20);
        parsed.circle = (payload[4] & 0x40); parsed.triangle = (payload[4] & 0x80);

        uint8_t buttons1 = payload[5];
        parsed.l1 = (buttons1 & 0x01); parsed.r1 = (buttons1 & 0x02);
        parsed.l2Button = (buttons1 & 0x04); parsed.r2Button = (buttons1 & 0x08);
        parsed.create = (buttons1 & 0x10); parsed.options = (buttons1 & 0x20);
        parsed.l3 = (buttons1 & 0x40); parsed.r3 = (buttons1 & 0x80);

        uint8_t buttons2 = payload[6];
        parsed.ps = (buttons2 & 0x01); parsed.touchpad = (buttons2 & 0x02);
        parsed.l2 = payload[7]; parsed.r2 = payload[8];
        return true;
    }

    bool ParseDualSenseBtFullReport(const uint8_t* data, size_t len, DualSenseParsedState& parsed)
    {
        if (len < 78 || data[0] != 0x31) return false;
        const uint8_t* payload = data + 1;
        parsed.lx = payload[1]; parsed.ly = payload[2];
        parsed.rx = payload[3]; parsed.ry = payload[4];
        parsed.l2 = payload[5]; parsed.r2 = payload[6];

        uint8_t buttons0 = payload[8];
        uint8_t buttons1 = payload[9];
        uint8_t buttons2 = payload[10];

        parsed.dpad = buttons0 & 0x0F;
        parsed.square = (buttons0 & 0x10); parsed.cross = (buttons0 & 0x20);
        parsed.circle = (buttons0 & 0x40); parsed.triangle = (buttons0 & 0x80);

        parsed.l1 = (buttons1 & 0x01); parsed.r1 = (buttons1 & 0x02);
        parsed.l2Button = (buttons1 & 0x04); parsed.r2Button = (buttons1 & 0x08);
        parsed.create = (buttons1 & 0x10); parsed.options = (buttons1 & 0x20);
        parsed.l3 = (buttons1 & 0x40); parsed.r3 = (buttons1 & 0x80);

        parsed.ps = (buttons2 & 0x01); parsed.touchpad = (buttons2 & 0x02); parsed.mute = (buttons2 & 0x04);
        return true;
    }

    bool DecodeDualSenseReport(const uint8_t* data, size_t len, XINPUT_STATE& out)
    {
        DualSenseParsedState parsed{};
        bool parsedOk = false;

        if (len >= 78 && data[0] == 0x31) parsedOk = ParseDualSenseBtFullReport(data, len, parsed);
        else if (len >= 64 && data[0] == 0x01) parsedOk = ParseDualSenseUsbReport(data, len, parsed);
        else if (len >= 10 && data[0] == 0x01) parsedOk = ParseDualSenseBtShortReport(data, len, parsed);

        if (!parsedOk) return false;

        out = {};
        XINPUT_GAMEPAD& pad = out.Gamepad;
        if (parsed.cross) pad.wButtons |= XINPUT_GAMEPAD_A;
        if (parsed.circle) pad.wButtons |= XINPUT_GAMEPAD_B;
        if (parsed.triangle) pad.wButtons |= XINPUT_GAMEPAD_Y;
        if (parsed.square) pad.wButtons |= XINPUT_GAMEPAD_X;
        if (parsed.l1) pad.wButtons |= XINPUT_GAMEPAD_LEFT_SHOULDER;
        if (parsed.r1) pad.wButtons |= XINPUT_GAMEPAD_RIGHT_SHOULDER;
        if (parsed.create) pad.wButtons |= XINPUT_GAMEPAD_BACK;
        if (parsed.options) pad.wButtons |= XINPUT_GAMEPAD_START;
        if (parsed.l3) pad.wButtons |= XINPUT_GAMEPAD_LEFT_THUMB;
        if (parsed.r3) pad.wButtons |= XINPUT_GAMEPAD_RIGHT_THUMB;
        if (parsed.touchpad) pad.wButtons |= XINPUT_GAMEPAD_BACK;
#ifdef XINPUT_GAMEPAD_GUIDE
        if (parsed.ps) pad.wButtons |= XINPUT_GAMEPAD_GUIDE;
#endif
        ApplySonyHat(parsed.dpad, pad.wButtons);
        pad.bLeftTrigger = parsed.l2;
        pad.bRightTrigger = parsed.r2;
        pad.sThumbLX = SonyStickFromByte(parsed.lx);
        pad.sThumbLY = SonyStickFromByte(parsed.ly, true);
        pad.sThumbRX = SonyStickFromByte(parsed.rx);
        pad.sThumbRY = SonyStickFromByte(parsed.ry, true);
        return true;
    }

    // --- DualShock 4 Parsing ---
    bool DecodeDualShock4Report(const uint8_t* data, size_t len, XINPUT_STATE& out)
    {
        if (len < 10) return false;
        const uint8_t* payload = nullptr;
        if (data[0] == 0x11 && len >= 78) payload = data + 3; // BT
        else if (data[0] == 0x01 && len >= 9) payload = data + 1; // USB
        else return false;

        out = {};
        XINPUT_GAMEPAD& pad = out.Gamepad;
        pad.sThumbLX = SonyStickFromByte(payload[0]);
        pad.sThumbLY = SonyStickFromByte(payload[1], true);
        pad.sThumbRX = SonyStickFromByte(payload[2]);
        pad.sThumbRY = SonyStickFromByte(payload[3], true);

        uint8_t btn1 = payload[4];
        uint8_t btn2 = payload[5];
        uint8_t btn3 = payload[6];

        ApplySonyHat(btn1 & 0x0F, pad.wButtons);
        if (btn1 & 0x10) pad.wButtons |= XINPUT_GAMEPAD_X; // Square
        if (btn1 & 0x20) pad.wButtons |= XINPUT_GAMEPAD_A; // Cross
        if (btn1 & 0x40) pad.wButtons |= XINPUT_GAMEPAD_B; // Circle
        if (btn1 & 0x80) pad.wButtons |= XINPUT_GAMEPAD_Y; // Triangle
        if (btn2 & 0x01) pad.wButtons |= XINPUT_GAMEPAD_LEFT_SHOULDER;
        if (btn2 & 0x02) pad.wButtons |= XINPUT_GAMEPAD_RIGHT_SHOULDER;
        if (btn2 & 0x10) pad.wButtons |= XINPUT_GAMEPAD_BACK;  // Share
        if (btn2 & 0x20) pad.wButtons |= XINPUT_GAMEPAD_START; // Options
        if (btn2 & 0x40) pad.wButtons |= XINPUT_GAMEPAD_LEFT_THUMB;
        if (btn2 & 0x80) pad.wButtons |= XINPUT_GAMEPAD_RIGHT_THUMB;
        if (btn3 & 0x01) pad.wButtons |= XINPUT_GAMEPAD_GUIDE; // PS

        pad.bLeftTrigger = payload[7];
        pad.bRightTrigger = payload[8];
        return true;
    }

    // --- Unified Processing ---
    void ConfigureSonyDevice(SonyDevice& dev)
    {
        if (dev.type == SonyControllerType::DualSense && dev.bluetooth && dev.caps.FeatureReportByteLength > 0)
        {
            std::vector<uint8_t> feature(dev.caps.FeatureReportByteLength, 0);
            feature[0] = 0x05;
            if (!HidD_GetFeature(dev.handle, feature.data(), static_cast<ULONG>(feature.size())))
                LOG_WARN("DualSense: Failed to request feature report 0x05 for %ls (err=%lu)", dev.path.c_str(), GetLastError());
        }
    }

    void SonyReadLoop(SonyDevice* dev)
    {
        while (dev->running.load())
        {
            DWORD bytesRead = 0;
            if (!ReadFile(dev->handle, dev->inputBuffer.data(), static_cast<DWORD>(dev->inputBuffer.size()), &bytesRead, nullptr))
            {
                DWORD err = GetLastError();
                if (!dev->running.load()) break;
                if (err == ERROR_OPERATION_ABORTED || err == ERROR_DEVICE_NOT_CONNECTED) break;
                Sleep(5);
                continue;
            }

            if (bytesRead == 0) continue;

            XINPUT_STATE xState{};
            bool parsed = false;

            if (dev->type == SonyControllerType::DualSense)
                parsed = DecodeDualSenseReport(dev->inputBuffer.data(), bytesRead, xState);
            else if (dev->type == SonyControllerType::DualShock4)
                parsed = DecodeDualShock4Report(dev->inputBuffer.data(), bytesRead, xState);

            if (parsed)
            {
                bool swapped = false;
                if (BaseHook::Hooks::SubmitVirtualGamepadState(GamepadInputSource::SonyHID, dev, xState, true, &swapped) && swapped)
                {
                    LOG_INFO("SonyHID: Switched virtual XInput source to %ls (%s)", dev->path.c_str(), dev->bluetooth ? "Bluetooth" : "USB");
                }
            }
        }
        dev->running = false;
        g_sonyPendingRefresh = true;
        BaseHook::Hooks::ResetVirtualGamepad(GamepadInputSource::SonyHID, dev);
    }

    void RefreshSonyDevices()
    {
        if (!g_sonyInitialized.load()) return;

        GUID hidGuid;
        HidD_GetHidGuid(&hidGuid);
        HDEVINFO devInfo = SetupDiGetClassDevs(&hidGuid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
        if (devInfo == INVALID_HANDLE_VALUE) return;

        std::lock_guard<std::mutex> lock(g_sonyDeviceMutex);
        for (auto& dev : g_sonyDevices) dev->seenInRefresh = false;

        SP_DEVICE_INTERFACE_DATA interfaceData{ sizeof(SP_DEVICE_INTERFACE_DATA) };
        for (DWORD index = 0; SetupDiEnumDeviceInterfaces(devInfo, nullptr, &hidGuid, index, &interfaceData); ++index)
        {
            DWORD requiredSize = 0;
            SetupDiGetDeviceInterfaceDetail(devInfo, &interfaceData, nullptr, 0, &requiredSize, nullptr);
            if (requiredSize == 0) continue;

            std::vector<BYTE> detailBuffer(requiredSize);
            auto detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA*>(detailBuffer.data());
            detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
            if (!SetupDiGetDeviceInterfaceDetail(devInfo, &interfaceData, detail, requiredSize, nullptr, nullptr)) continue;

            std::wstring path = ToWideString(detail->DevicePath);
            if (path.empty()) continue;

            HANDLE handle = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (handle == INVALID_HANDLE_VALUE)
            {
                handle = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (handle == INVALID_HANDLE_VALUE) continue;
            }

            HIDD_ATTRIBUTES attrs{ sizeof(HIDD_ATTRIBUTES) };
            if (!HidD_GetAttributes(handle, &attrs) || attrs.VendorID != kSonyVendorId)
            {
                CloseHandle(handle);
                continue;
            }

            SonyControllerType type = SonyControllerType::Unknown;
            if (attrs.ProductID == kDualSenseProductId) type = SonyControllerType::DualSense;
            else if (std::find(kDualShock4ProductIds.begin(), kDualShock4ProductIds.end(), attrs.ProductID) != kDualShock4ProductIds.end()) type = SonyControllerType::DualShock4;

            if (type == SonyControllerType::Unknown)
            {
                CloseHandle(handle);
                continue;
            }

            PHIDP_PREPARSED_DATA parsed = nullptr;
            HIDP_CAPS caps{};
            if (!HidD_GetPreparsedData(handle, &parsed) || HidP_GetCaps(parsed, &caps) != HIDP_STATUS_SUCCESS || caps.InputReportByteLength == 0)
            {
                if (parsed) HidD_FreePreparsedData(parsed);
                CloseHandle(handle);
                continue;
            }
            HidD_FreePreparsedData(parsed);

            // Check if already tracked
            bool alreadyTracked = false;
            for (auto& dev : g_sonyDevices)
            {
                if (_wcsicmp(dev->path.c_str(), path.c_str()) == 0)
                {
                    if (dev->running.load())
                    {
                        dev->seenInRefresh = true;
                        alreadyTracked = true;
                    }
                    break;
                }
            }

            if (alreadyTracked)
            {
                CloseHandle(handle);
                continue;
            }

            bool bluetooth = false;
            std::wstring lower = path;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
            if (lower.find(L"bthenum") != std::wstring::npos) bluetooth = true;

            auto dev = std::make_unique<SonyDevice>();
            dev->path = path;
            dev->handle = handle;
            dev->caps = caps;
            dev->bluetooth = bluetooth;
            dev->type = type;
            dev->pid = attrs.ProductID;
            dev->inputBuffer.resize(caps.InputReportByteLength);
            dev->running = true;
            dev->seenInRefresh = true;

            ConfigureSonyDevice(*dev);
            dev->readerThread = std::thread(SonyReadLoop, dev.get());

            LOG_INFO("SonyHID: Tracking %ls (%s)", dev->path.c_str(), dev->bluetooth ? "Bluetooth" : "USB");
            g_sonyDevices.push_back(std::move(dev));
        }

        auto it = g_sonyDevices.begin();
        while (it != g_sonyDevices.end())
        {
            if (!(*it)->seenInRefresh)
            {
                LOG_INFO("SonyHID: Removing %ls", (*it)->path.c_str());
                (*it)->Stop();
                it = g_sonyDevices.erase(it);
            }
            else ++it;
        }

        SetupDiDestroyDeviceInfoList(devInfo);
        g_lastSonyRefreshMs.store(GetTickCount64(), std::memory_order_relaxed);
    }

    void PumpSonyHotplug(bool forceImmediate)
    {
        if (!g_sonyInitialized.load(std::memory_order_acquire)) return;

        bool shouldRefresh = forceImmediate;
        if (!shouldRefresh)
        {
            if (g_sonyPendingRefresh.exchange(false, std::memory_order_acq_rel)) shouldRefresh = true;
            else if (!HasSonyDevices())
            {
                unsigned long long last = g_lastSonyRefreshMs.load(std::memory_order_relaxed);
                if (last == 0 || GetTickCount64() - last >= kSonyAutoRefreshMs) shouldRefresh = true;
            }
        }
        if (shouldRefresh) RefreshSonyDevices();
    }

    void InitializeSonySupport()
    {
        bool expected = false;
        if (!g_sonyInitialized.compare_exchange_strong(expected, true)) return;
        PumpSonyHotplug(true);
    }

    void ShutdownSonySupport()
    {
        if (!g_sonyInitialized.exchange(false)) return;
        std::lock_guard<std::mutex> lock(g_sonyDeviceMutex);
        for (auto& dev : g_sonyDevices) dev->Stop();
        g_sonyDevices.clear();
    }

    void OnSonyDeviceChange()
    {
        if (!g_sonyInitialized.load()) return;
        g_sonyPendingRefresh.store(true, std::memory_order_release);
        PumpSonyHotplug(true);
    }
}

// --- Helpers ---

void ProcessDeviceState(IDirectInputDevice8* pDevice, DWORD cbData, LPVOID lpvData)
{
    if (lpvData == nullptr) return;

    DeviceInfo info;
    bool bFound = false;

    // 1. Fast Lookup
    {
        std::shared_lock<std::shared_mutex> lock(g_dinput_mutex);
        auto it = g_Devices.find(pDevice);
        if (it != g_Devices.end()) {
            info = it->second;
            bFound = true;
        } else if (g_FailedDevices.count(pDevice)) {
            return;
        }
    }

    // 2. Initialization (if not found)
    if (!bFound)
    {
        std::unique_lock<std::shared_mutex> lock(g_dinput_mutex);
        if (g_Devices.count(pDevice)) {
            info = g_Devices[pDevice];
            bFound = true;
        }
        else if (g_FailedDevices.count(pDevice) == 0)
        {
            DIDEVCAPS caps = { sizeof(caps) };
            if (SUCCEEDED(pDevice->GetCapabilities(&caps)))
            {
                DIPROPDWORD dipdw;
                dipdw.diph.dwSize = sizeof(DIPROPDWORD);
                dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
                dipdw.diph.dwObj = 0;
                dipdw.diph.dwHow = DIPH_DEVICE;
                if (SUCCEEDED(pDevice->GetProperty(DIPROP_VIDPID, &dipdw.diph))) {
                    info.vid = LOWORD(dipdw.dwData);
                    info.pid = HIWORD(dipdw.dwData);
                }

                info.type = LOBYTE(caps.dwDevType);

                if (info.type != DI8DEVTYPE_MOUSE && info.type != DI8DEVTYPE_KEYBOARD)
                {
                    const DWORD axisOffsets[] = { DIJOFS_X, DIJOFS_Y, DIJOFS_Z, DIJOFS_RX, DIJOFS_RY, DIJOFS_RZ };
                    for (int i = 0; i < 6; i++)
                    {
                        DIPROPRANGE range;
                        range.diph.dwSize = sizeof(DIPROPRANGE);
                        range.diph.dwHeaderSize = sizeof(DIPROPHEADER);
                        range.diph.dwHow = DIPH_BYOFFSET;
                        range.diph.dwObj = axisOffsets[i];

                        if (SUCCEEDED(pDevice->GetProperty(DIPROP_RANGE, &range.diph))) {
                            info.axes[i].min = range.lMin;
                            info.axes[i].max = range.lMax;
                        }
                    }
                }

                g_Devices[pDevice] = info;
                bFound = true;
            }
            else
            {
                g_FailedDevices.insert(pDevice);
            }
        }
    }

    if (bFound)
    {
        // --- Gamepad / Joystick Processing ---
        if (info.type != DI8DEVTYPE_MOUSE && info.type != DI8DEVTYPE_KEYBOARD)
        {
            if (cbData >= sizeof(DIJOYSTATE))
            {
                // --- FIX: Inject HID Data for Wireless PS4/PS5 ---
                bool bInjected = false;
                {
                    std::lock_guard<std::mutex> lock(g_virtualPadMutex);
                    if (g_virtualPad.hasData)
                    {
                        if (g_virtualPad.source == GamepadInputSource::SonyHID && info.vid == 0x054C)
                        {
                            // Verify matching PID to ensure we inject into the correct device
                            auto* dev = static_cast<SonyDevice*>(g_virtualPad.context);
                            if (dev && dev->pid == info.pid)
                            {
                                ApplyHIDStateToDI(g_virtualPad.state, info, lpvData, cbData);
                                bInjected = true;
                            }
                        }
                    }
                }

                bool isPrimary = false;
                if (g_PrimaryDevice == nullptr || g_PrimaryDevice == pDevice) {
                    g_PrimaryDevice = pDevice;
                    isPrimary = true;
                }

                auto* js = static_cast<DIJOYSTATE*>(lpvData);
                DIJOYSTATE2 state{};
                const size_t copySize = std::min<size_t>(cbData, sizeof(DIJOYSTATE2));
                std::memcpy(&state, lpvData, copySize);

                if (isPrimary)
                {
                    if (!bInjected)
                    {
                        XINPUT_STATE translated = BuildVirtualXInputState(state, info);
                        bool swapped = false;
                        if (BaseHook::Hooks::SubmitVirtualGamepadState(GamepadInputSource::HookedDevice, pDevice, translated, true, &swapped) && swapped)
                        {
                            LOG_INFO("DI8: Switched virtual XInput source to hooked DirectInput device %p.", pDevice);
                        }
                    }
                }

                if (BaseHook::Data::bFixDirectInput)
                {
                    std::swap(js->rgbButtons[0], js->rgbButtons[1]); // A <-> X
                    std::swap(js->rgbButtons[2], js->rgbButtons[3]); // B <-> Y
                    std::swap(js->rgbButtons[4], js->rgbButtons[6]); // L1 <-> L2
                    std::swap(js->rgbButtons[5], js->rgbButtons[7]); // R1 <-> R2
                }
            }
        }
        // --- Mouse Processing ---
        else if (info.type == DI8DEVTYPE_MOUSE && BaseHook::Data::bIsInitialized)
        {
            if (cbData == sizeof(DIMOUSESTATE) || cbData == sizeof(DIMOUSESTATE2))
            {
                auto* ms = static_cast<DIMOUSESTATE*>(lpvData);
                std::lock_guard<std::mutex> queueLock(g_input_queue_mutex);

                g_mouse_buffer.lX += ms->lX;
                g_mouse_buffer.lY += ms->lY;
                g_mouse_buffer.lZ += ms->lZ;

                for (int i = 0; i < 4; i++) g_mouse_buffer.rgbButtons[i] = ms->rgbButtons[i];
                if (cbData == sizeof(DIMOUSESTATE2)) {
                    auto* ms2 = static_cast<DIMOUSESTATE2*>(lpvData);
                    for (int i = 4; i < 8; i++) g_mouse_buffer.rgbButtons[i] = ms2->rgbButtons[i];
                }
            }
        }

        // --- Blocking Logic ---
        if (BaseHook::Data::bBlockInput)
        {
            if (info.type == DI8DEVTYPE_MOUSE || info.type == DI8DEVTYPE_KEYBOARD)
            {
                std::memset(lpvData, 0, cbData);
            }
            else
            {
                if (cbData >= sizeof(DIJOYSTATE))
                {
                    auto* js = static_cast<DIJOYSTATE*>(lpvData);
                    std::memset(lpvData, 0, cbData); 
                    for(int i=0; i<4; i++) js->rgdwPOV[i] = (DWORD)-1; 
                    js->lX  = info.axes[0].GetCenter();
                    js->lY  = info.axes[1].GetCenter();
                    js->lZ  = info.axes[2].GetCenter();
                    js->lRx = info.axes[3].GetCenter();
                    js->lRy = info.axes[4].GetCenter();
                    js->lRz = info.axes[5].GetCenter();
                }
            }
        }
    }
}

void ProcessDeviceData(IDirectInputDevice8* pDevice, DWORD cbObjectData, LPDIDEVICEOBJECTDATA rgdod, LPDWORD pdwInOut, DWORD dwFlags)
{
    if ((dwFlags & DIGDD_PEEK) == 0 && pdwInOut && *pdwInOut > 0 && rgdod)
    {
        BYTE type = 0;
        {
            std::shared_lock<std::shared_mutex> lock(g_dinput_mutex);
            auto it = g_Devices.find(pDevice);
            if (it != g_Devices.end()) type = it->second.type;
        }

        if (type != 0)
        {
            if (type != DI8DEVTYPE_MOUSE && type != DI8DEVTYPE_KEYBOARD)
            {
                if (BaseHook::Data::bFixDirectInput)
                {
                    for (DWORD i = 0; i < *pdwInOut; i++)
                    {
                        switch (rgdod[i].dwOfs)
                        {
                        case 48: rgdod[i].dwOfs = 49; break; // A <-> X
                        case 49: rgdod[i].dwOfs = 48; break;
                        case 50: rgdod[i].dwOfs = 51; break; // B <-> Y
                        case 51: rgdod[i].dwOfs = 50; break;
                        case 52: rgdod[i].dwOfs = 54; break; // L1 <-> L2
                        case 54: rgdod[i].dwOfs = 52; break;
                        case 53: rgdod[i].dwOfs = 55; break; // R1 <-> R2
                        case 55: rgdod[i].dwOfs = 53; break;
                        }
                    }
                }
            }

            if (BaseHook::Data::bBlockInput) {
                 *pdwInOut = 0;
            }
        }
    }
}

// --- Hooks ---

HRESULT STDMETHODCALLTYPE hkGetDeviceState(IDirectInputDevice8* pDevice, DWORD cbData, LPVOID lpvData)
{
    IDirectInputDevice8_GetDeviceState_t oGetDeviceState = nullptr;
    {
        void** vtable = *reinterpret_cast<void***>(pDevice);
        void* pFunc = vtable[9];
        std::shared_lock<std::shared_mutex> lock(g_dinput_mutex);
        auto it = g_DeviceMethodTrampolines.find(pFunc);
        if (it != g_DeviceMethodTrampolines.end())
            oGetDeviceState = static_cast<IDirectInputDevice8_GetDeviceState_t>(it->second);
    }

    if (!oGetDeviceState) return DIERR_GENERIC;

    if (g_isPrivatePolling)
        return oGetDeviceState(pDevice, cbData, lpvData);

    HRESULT hr = oGetDeviceState(pDevice, cbData, lpvData);

    if (FAILED(hr)) {
        if (g_PrimaryDevice == pDevice) {
            g_PrimaryDevice = nullptr;
            g_lastAuthoritativeUpdateMs.store(0, std::memory_order_relaxed);
            ResetVirtualPad(GamepadInputSource::HookedDevice, pDevice);
            SchedulePrivateRefresh();
        }
        return hr;
    }

    ProcessDeviceState(pDevice, cbData, lpvData);
    return hr;
}

HRESULT STDMETHODCALLTYPE hkGetDeviceData(IDirectInputDevice8* pDevice, DWORD cbObjectData, LPDIDEVICEOBJECTDATA rgdod, LPDWORD pdwInOut, DWORD dwFlags)
{
    IDirectInputDevice8_GetDeviceData_t oGetDeviceData = nullptr;
    {
        void** vtable = *reinterpret_cast<void***>(pDevice);
        void* pFunc = vtable[10];
        std::shared_lock<std::shared_mutex> lock(g_dinput_mutex);
        auto it = g_DeviceMethodTrampolines.find(pFunc);
        if (it != g_DeviceMethodTrampolines.end())
            oGetDeviceData = static_cast<IDirectInputDevice8_GetDeviceData_t>(it->second);
    }

    if (!oGetDeviceData) return DIERR_GENERIC;

    if (g_isPrivatePolling)
        return oGetDeviceData(pDevice, cbObjectData, rgdod, pdwInOut, dwFlags);

    HRESULT hr = oGetDeviceData(pDevice, cbObjectData, rgdod, pdwInOut, dwFlags);
    if (SUCCEEDED(hr)) ProcessDeviceData(pDevice, cbObjectData, rgdod, pdwInOut, dwFlags);
    return hr;
}

HRESULT STDMETHODCALLTYPE hkSetCooperativeLevel(IDirectInputDevice8* pDevice, HWND hWnd, DWORD dwFlags)
{
    if (dwFlags & DISCL_EXCLUSIVE)
    {
        dwFlags &= ~DISCL_EXCLUSIVE;
        dwFlags |= DISCL_NONEXCLUSIVE;
    }

    IDirectInputDevice8_SetCooperativeLevel_t oSetCooperativeLevel = nullptr;
    {
        void** vtable = *reinterpret_cast<void***>(pDevice);
        void* pFunc = vtable[13];
        std::shared_lock<std::shared_mutex> lock(g_dinput_mutex);
        auto it = g_DeviceMethodTrampolines.find(pFunc);
        if (it != g_DeviceMethodTrampolines.end())
            oSetCooperativeLevel = static_cast<IDirectInputDevice8_SetCooperativeLevel_t>(it->second);
    }

    if (!oSetCooperativeLevel) return DIERR_GENERIC;

    return oSetCooperativeLevel(pDevice, hWnd, dwFlags);
}

void HookDeviceMethods(IDirectInputDevice8* device)
{
    void** vtable = *reinterpret_cast<void***>(device);
    std::unique_lock<std::shared_mutex> lock(g_dinput_mutex);

    void* pGetDeviceState = vtable[9];
    void* pGetDeviceData = vtable[10];
    void* pSetCooperativeLevel = vtable[13];

    if (g_DeviceMethodTrampolines.find(pGetDeviceState) == g_DeviceMethodTrampolines.end())
    {
        void* pTrampoline = nullptr;
        if (MH_CreateHook(pGetDeviceState, &hkGetDeviceState, &pTrampoline) == MH_OK &&
            MH_EnableHook(pGetDeviceState) == MH_OK)
        {
            g_DeviceMethodTrampolines[pGetDeviceState] = pTrampoline;
            g_hooked_functions.insert(pGetDeviceState);
        }
    }

    if (g_DeviceMethodTrampolines.find(pGetDeviceData) == g_DeviceMethodTrampolines.end())
    {
        void* pTrampoline = nullptr;
        if (MH_CreateHook(pGetDeviceData, &hkGetDeviceData, &pTrampoline) == MH_OK &&
            MH_EnableHook(pGetDeviceData) == MH_OK)
        {
            g_DeviceMethodTrampolines[pGetDeviceData] = pTrampoline;
            g_hooked_functions.insert(pGetDeviceData);
        }
    }

    if (g_DeviceMethodTrampolines.find(pSetCooperativeLevel) == g_DeviceMethodTrampolines.end())
    {
        void* pTrampoline = nullptr;
        if (MH_CreateHook(pSetCooperativeLevel, &hkSetCooperativeLevel, &pTrampoline) == MH_OK &&
            MH_EnableHook(pSetCooperativeLevel) == MH_OK)
        {
            g_DeviceMethodTrampolines[pSetCooperativeLevel] = pTrampoline;
            g_hooked_functions.insert(pSetCooperativeLevel);
        }
    }
}

static void ProcessCreateDeviceResult(HRESULT hr, IDirectInput8* pDI, LPDIRECTINPUTDEVICE8* lplpDirectInputDevice)
{
    if (SUCCEEDED(hr) && lplpDirectInputDevice && *lplpDirectInputDevice)
    {
        if (pDI == g_privateDI) return;
        HookDeviceMethods(*lplpDirectInputDevice);
    }
}

HRESULT STDMETHODCALLTYPE hkCreateDeviceA(IDirectInput8* pDI, REFGUID rguid, LPDIRECTINPUTDEVICE8* lplpDirectInputDevice, LPUNKNOWN pUnkOuter)
{
    IDirectInput8_CreateDevice_t oCreateDeviceA = nullptr;
    {
        void** vtable = *reinterpret_cast<void***>(pDI);
        void* pFunc = vtable[3];
        std::shared_lock<std::shared_mutex> lock(g_dinput_mutex);
        auto it = g_FactoryMethodTrampolines.find(pFunc);
        if (it != g_FactoryMethodTrampolines.end())
            oCreateDeviceA = static_cast<IDirectInput8_CreateDevice_t>(it->second);
    }

    if (!oCreateDeviceA) return DIERR_GENERIC;

    HRESULT hr = oCreateDeviceA(pDI, rguid, lplpDirectInputDevice, pUnkOuter);
    ProcessCreateDeviceResult(hr, pDI, lplpDirectInputDevice);
    return hr;
}

HRESULT STDMETHODCALLTYPE hkCreateDeviceW(IDirectInput8* pDI, REFGUID rguid, LPDIRECTINPUTDEVICE8* lplpDirectInputDevice, LPUNKNOWN pUnkOuter)
{
    IDirectInput8_CreateDevice_t oCreateDeviceW = nullptr;
    {
        void** vtable = *reinterpret_cast<void***>(pDI);
        void* pFunc = vtable[3];
        std::shared_lock<std::shared_mutex> lock(g_dinput_mutex);
        auto it = g_FactoryMethodTrampolines.find(pFunc);
        if (it != g_FactoryMethodTrampolines.end())
            oCreateDeviceW = static_cast<IDirectInput8_CreateDevice_t>(it->second);
    }

    if (!oCreateDeviceW) return DIERR_GENERIC;

    HRESULT hr = oCreateDeviceW(pDI, rguid, lplpDirectInputDevice, pUnkOuter);
    ProcessCreateDeviceResult(hr, pDI, lplpDirectInputDevice);
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

        if (g_FactoryMethodTrampolines.find(pCreateDevice) == g_FactoryMethodTrampolines.end())
        {
            if (IsEqualGUID(riidltf, IID_IDirectInput8A_Local))
            {
                void* pTrampoline = nullptr;
                if (MH_CreateHook(pCreateDevice, &hkCreateDeviceA, &pTrampoline) == MH_OK &&
                    MH_EnableHook(pCreateDevice) == MH_OK)
                {
                    g_FactoryMethodTrampolines[pCreateDevice] = pTrampoline;
                    g_hooked_functions.insert(pCreateDevice);
                }
            }
            else if (IsEqualGUID(riidltf, IID_IDirectInput8W_Local))
            {
                void* pTrampoline = nullptr;
                if (MH_CreateHook(pCreateDevice, &hkCreateDeviceW, &pTrampoline) == MH_OK &&
                    MH_EnableHook(pCreateDevice) == MH_OK)
                {
                    g_FactoryMethodTrampolines[pCreateDevice] = pTrampoline;
                    g_hooked_functions.insert(pCreateDevice);
                }
            }
        }
    }
    return hr;
}


    void CleanupDirectInput()
    {
        ShutdownSonySupport();
        std::unique_lock<std::shared_mutex> lock(g_dinput_mutex);
        g_Devices.clear();
        g_FailedDevices.clear();
        g_hooked_functions.clear();
        g_DeviceMethodTrampolines.clear();
        g_FactoryMethodTrampolines.clear();
        g_PrimaryDevice = nullptr;
        g_lastAuthoritativeUpdateMs.store(0, std::memory_order_relaxed);
        lock.unlock();

        {
            std::lock_guard<std::mutex> privateLock(g_privateDeviceMutex);
            for (auto& dev : g_privateDevices)
                ReleasePrivateDevice(dev);
            g_privateDevices.clear();

            if (g_privateDI)
            {
                g_privateDI->Release();
                g_privateDI = nullptr;
            }
        }

        ResetVirtualPad();

        g_privateWindow = nullptr;
        g_pendingPrivateRefresh.store(false);
        g_isRefreshing = false;
        g_lastPrivateEnum = {};
        g_lastPrivateSampleLog = {};
    }

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
            }
        }

        InitializeSonySupport();
    }

    void NotifyDirectInputWindow(HWND hWnd)
    {
        {
            std::lock_guard<std::mutex> lock(g_privateDeviceMutex);
            g_privateWindow = hWnd;
        }

        if (hWnd)
        {
            LOG_INFO("DI8: Private polling enabled for window %p.", hWnd);
            SchedulePrivateRefresh();
            RefreshPrivateDevices(true);
        }
    }

    void HandleDeviceChange(WPARAM wParam, LPARAM)
    {
        switch (wParam)
        {
        case DBT_DEVNODES_CHANGED:
        case DBT_DEVICEARRIVAL:
        case DBT_DEVICEREMOVECOMPLETE:
            LOG_INFO("DI8: Controller change detected (%u).", static_cast<unsigned>(wParam));
            SchedulePrivateRefresh();
            OnSonyDeviceChange();
            break;
        default:
            break;
        }
    }

    void ApplyBufferedInput()
    {
        PumpSonyHotplug(false);
        PollPrivateDevicesFallback();

    {
        std::lock_guard<std::mutex> lock(g_input_queue_mutex);
        ImGuiIO& io = ImGui::GetIO();

        if (BaseHook::Data::bImGuiMouseButtonsFromDirectInput)
        {
            // Overlay mouse movement is always Win32/WndProc-driven.
            // Only inject wheel + buttons from DirectInput when configured.
            if (g_mouse_buffer.lZ != 0)
            {
                io.MouseWheel += (float)g_mouse_buffer.lZ / 120.0f;

                g_mouse_buffer.lZ = 0;
            }

            for (int i = 0; i < 8; i++)
                io.MouseDown[i] = (g_mouse_buffer.rgbButtons[i] & 0x80) != 0;
        }
    }

    ApplyVirtualPadToImGui();
    }

    bool SubmitVirtualGamepadState(GamepadInputSource source, void* context, const XINPUT_STATE& state, bool markAuthoritative, bool* outSourceChanged)
    {
        bool swapped = false;
        bool accepted = UpdateVirtualPad(source, context, state, swapped);
        if (!accepted)
        {
            if (outSourceChanged)
                *outSourceChanged = false;
            return false;
        }

        if (outSourceChanged)
            *outSourceChanged = swapped;

        if (markAuthoritative)
            g_lastAuthoritativeUpdateMs.store(GetTickCount64(), std::memory_order_relaxed);

        return true;
    }

    void ResetVirtualGamepad(GamepadInputSource sourceFilter, void* contextFilter)
    {
        ResetVirtualPad(sourceFilter, contextFilter);
        if (sourceFilter != GamepadInputSource::PrivateFallback)
        {
            g_lastAuthoritativeUpdateMs.store(0, std::memory_order_relaxed);
        }
    }

    bool TryGetVirtualXInputState(DWORD dwUserIndex, XINPUT_STATE* pState)
    {
        if (dwUserIndex != 0 || !pState)
            return false;

        return TryCopyVirtualPad(pState);
    }
}}
