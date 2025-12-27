#pragma once
#include <string>
#include <functional>
#include <windows.h>
#include <xinput.h>

#ifndef XINPUT_GAMEPAD_GUIDE
#define XINPUT_GAMEPAD_GUIDE 0x0400
#endif

struct KeyBind
{
    // Pseudo-keys for Triggers (handled manually)
    static constexpr unsigned int PAD_L_TRIGGER = 0x10000;
    static constexpr unsigned int PAD_R_TRIGGER = 0x20000;

    unsigned int KeyboardKey = 0; // Virtual Key Code
    bool Ctrl = false;
    bool Shift = false;
    bool Alt = false;
    
    unsigned int ControllerKey = 0; // XInput Button Flag

    // Function signature matching BaseHook::Hooks::TryGetVirtualXInputState
    using InputProvider_t = std::function<bool(DWORD, XINPUT_STATE*)>;
    static void SetInputProvider(InputProvider_t provider);

    KeyBind() = default;
    
    KeyBind(unsigned int kbKey, bool c, bool s, bool a, unsigned int padKey)
        : KeyboardKey(kbKey), Ctrl(c), Shift(s), Alt(a), ControllerKey(padKey) {}

    // Legacy-style constructor
    explicit KeyBind(unsigned int k, bool c = false, bool s = false, bool a = false)
        : KeyboardKey(k), Ctrl(c), Shift(s), Alt(a), ControllerKey(0) {}

    bool operator==(const KeyBind& other) const {
        return KeyboardKey == other.KeyboardKey && Ctrl == other.Ctrl && Shift == other.Shift && Alt == other.Alt && ControllerKey == other.ControllerKey;
    }
    bool operator!=(const KeyBind& other) const { return !(*this == other); }

    // Returns true if EITHER the keyboard bind OR the controller bind is pressed
    bool IsPressed(bool strict = false) const;

    // Returns true if the provided message matches the KEYBOARD bind
    bool IsPressedEvent(unsigned int msg, uintptr_t wParam, bool strict = false) const;

    // Returns human readable string (e.g. "Ctrl + A / Pad LT+RT")
    std::string ToString() const;

    // Helper: Get human readable name for a VK code
    static std::string GetKeyName(unsigned int vk);

    // Wrapper to get state from provider
    static bool GetControllerState(unsigned int userIndex, XINPUT_STATE* state);

    // Helper to convert XInput state to our flags (buttons + triggers)
    static unsigned int GetGamepadFlags(const XINPUT_STATE& state);

private:
    static InputProvider_t s_InputProvider;
};