#pragma once
#include <Windows.h>
#include <XInput.h>

namespace BaseHook::Hooks
{
    void InitDirectInput();
    void InitXInput();
    void CleanupDirectInput();
    void NotifyDirectInputWindow(HWND hWnd);
    void ApplyBufferedInput();
    
    enum class GamepadInputSource : uint8_t
    {
        None,
        HookedDevice,
        PrivateFallback,
        SonyHID 
    };

    bool TryGetVirtualXInputState(DWORD dwUserIndex, XINPUT_STATE* pState);
    bool SubmitVirtualGamepadState(GamepadInputSource source, void* context, const XINPUT_STATE& state, bool markAuthoritative, bool* outSourceChanged = nullptr);
    void ResetVirtualGamepad(GamepadInputSource sourceFilter = GamepadInputSource::None, void* contextFilter = nullptr);
}
