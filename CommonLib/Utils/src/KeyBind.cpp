#include "KeyBind.h"
#include <Windows.h>
#include <vector>
#include <cstdio>

// ReShade-based Key Name Lookup Table
static const char* g_KeyNames[256] = {
    "", "Left Mouse", "Right Mouse", "Cancel", "Middle Mouse", "X1 Mouse", "X2 Mouse", "", "Backspace", "Tab", "", "", "Clear", "Enter", "", "",
    "Shift", "Control", "Alt", "Pause", "Caps Lock", "", "", "", "", "", "", "Escape", "", "", "", "",
    "Space", "Page Up", "Page Down", "End", "Home", "Left Arrow", "Up Arrow", "Right Arrow", "Down Arrow", "Select", "", "", "Print Screen", "Insert", "Delete", "Help",
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "", "", "", "", "", "",
    "", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O",
    "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "Left Windows", "Right Windows", "Apps", "", "Sleep",
    "Numpad 0", "Numpad 1", "Numpad 2", "Numpad 3", "Numpad 4", "Numpad 5", "Numpad 6", "Numpad 7", "Numpad 8", "Numpad 9", "Numpad *", "Numpad +", "", "Numpad -", "Numpad Decimal", "Numpad /",
    "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12", "F13", "F14", "F15", "F16",
    "F17", "F18", "F19", "F20", "F21", "F22", "F23", "F24", "", "", "", "", "", "", "", "",
    "Num Lock", "Scroll Lock", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "Left Shift", "Right Shift", "Left Control", "Right Control", "Left Menu", "Right Menu", "Browser Back", "Browser Forward", "Browser Refresh", "Browser Stop", "Browser Search", "Browser Favorites", "Browser Home", "Volume Mute", "Volume Down", "Volume Up",
    "Next Track", "Previous Track", "Media Stop", "Media Play/Pause", "Mail", "Media Select", "Launch App 1", "Launch App 2", "", "", "OEM ;", "OEM +", "OEM ,", "OEM -", "OEM .", "OEM /",
    "OEM ~", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "OEM [", "OEM \\", "OEM ]", "OEM '", "OEM 8",
    "", "", "OEM <", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "Attn", "CrSel", "ExSel", "Erase EOF", "Play", "Zoom", "", "PA1", "OEM Clear", ""
};

std::string KeyBind::GetKeyName(unsigned int vk)
{
    if (vk >= 256) return "Unknown";
    // Localize/Adjust specific keys if needed (e.g. Pos1 for German), keeping simple for now
    const char* name = g_KeyNames[vk];
    if (name && *name) return name;
    
    char buf[16];
    snprintf(buf, sizeof(buf), "0x%02X", vk);
    return buf;
}

std::string KeyBind::ToString() const
{
    if (Key == 0) return "None";

    std::string str;
    if (Ctrl) str += "Ctrl + ";
    if (Shift) str += "Shift + ";
    if (Alt) str += "Alt + ";
    str += GetKeyName(Key);
    return str;
}

static bool IsKeyDown(int vk) {
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

bool KeyBind::IsPressed(bool strict) const
{
    if (Key == 0) return false;

    bool k = IsKeyDown(Key);
    bool c = IsKeyDown(VK_CONTROL);
    bool s = IsKeyDown(VK_SHIFT);
    bool a = IsKeyDown(VK_MENU);

    if (strict)
        return k && (Ctrl == c) && (Shift == s) && (Alt == a);
    else
        return k && (!Ctrl || c) && (!Shift || s) && (!Alt || a);
}

bool KeyBind::IsPressedEvent(unsigned int msg, uintptr_t wParam, bool strict) const
{
    if (Key == 0) return false;

    if (msg != WM_KEYDOWN && msg != WM_SYSKEYDOWN) return false;
    if (wParam != Key) return false;

    bool c = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    bool s = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    bool a = (GetKeyState(VK_MENU) & 0x8000) != 0;

    if (strict)
        return (Ctrl == c) && (Shift == s) && (Alt == a);
    else
        return (!Ctrl || c) && (!Shift || s) && (!Alt || a);
}