#pragma once
#include <string>
#include <cstdint>

struct KeyBind
{
    unsigned int Key = 0; // Virtual Key Code
    bool Ctrl = false;
    bool Shift = false;
    bool Alt = false;

    KeyBind() = default;
    KeyBind(unsigned int k, bool c = false, bool s = false, bool a = false)
        : Key(k), Ctrl(c), Shift(s), Alt(a) {}

    bool operator==(const KeyBind& other) const {
        return Key == other.Key && Ctrl == other.Ctrl && Shift == other.Shift && Alt == other.Alt;
    }
    bool operator!=(const KeyBind& other) const { return !(*this == other); }

    // Returns true if the key combination is currently physically pressed (Polling)
    // strict: if true, modifiers must match exactly (no extra modifiers allowed)
    bool IsPressed(bool strict = false) const;

    // Returns true if the provided message matches this bind
    bool IsPressedEvent(unsigned int msg, uintptr_t wParam, bool strict = false) const;

    // Returns human readable string (e.g. "Ctrl + A")
    std::string ToString() const;

    // Helper: Get human readable name for a VK code
    static std::string GetKeyName(unsigned int vk);
};