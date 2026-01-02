#include "KeyBind.h"
#include <Windows.h>
#include <xinput.h>
#include <cstdio>

KeyBind::InputProvider_t KeyBind::s_InputProvider = nullptr;

void KeyBind::SetInputProvider(InputProvider_t provider)
{
    s_InputProvider = provider;
}

bool KeyBind::GetControllerState(unsigned int userIndex, XINPUT_STATE* state)
{
    // Use the virtual provider if available (hooks into BaseHook's unified input)
    if (s_InputProvider)
        return s_InputProvider(userIndex, state);
    
    // Strict Mode: Do not fallback to raw XInput functions here.
    // We rely on the hook system to provide input to ensure overlay/game blocking works correctly.
    return false; 
}

unsigned int KeyBind::GetGamepadFlags(const XINPUT_STATE& state)
{
    unsigned int buttons = state.Gamepad.wButtons;
    // Map triggers to pseudo-buttons using standard threshold (30/255)
    // We use the XInput definition or a safe fallback
    const BYTE threshold = XINPUT_GAMEPAD_TRIGGER_THRESHOLD; 
    if (state.Gamepad.bLeftTrigger > threshold) buttons |= PAD_L_TRIGGER;
    if (state.Gamepad.bRightTrigger > threshold) buttons |= PAD_R_TRIGGER;
    return buttons;
}

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
    const char* name = g_KeyNames[vk];
    if (name && *name) return name;
    
    char buf[16];
    snprintf(buf, sizeof(buf), "0x%02X", vk);
    return buf;
}

std::string KeyBind::ToString() const
{
    std::string str;
    
    // Keyboard Part
    if (KeyboardKey != 0)
    {
        if (Ctrl) str += "Ctrl + ";
        if (Shift) str += "Shift + ";
        if (Alt) str += "Alt + ";
        str += GetKeyName(KeyboardKey);
    }

    // Controller Part
    if (ControllerKey != 0)
    {
        if (!str.empty()) str += " / ";
        
        str += "Pad ";
        bool first = true;
        auto add = [&](const char* name) {
            if (!first) str += "+";
            str += name;
            first = false;
        };

        const unsigned int k = ControllerKey;
        if (k & PAD_L_TRIGGER) add("LT");
        if (k & PAD_R_TRIGGER) add("RT");
        if (k & XINPUT_GAMEPAD_DPAD_UP) add("D-Up");
        if (k & XINPUT_GAMEPAD_DPAD_DOWN) add("D-Down");
        if (k & XINPUT_GAMEPAD_DPAD_LEFT) add("D-Left");
        if (k & XINPUT_GAMEPAD_DPAD_RIGHT) add("D-Right");
        if (k & XINPUT_GAMEPAD_START) add("Start");
        if (k & XINPUT_GAMEPAD_BACK) add("Back");
        if (k & XINPUT_GAMEPAD_LEFT_THUMB) add("L3");
        if (k & XINPUT_GAMEPAD_RIGHT_THUMB) add("R3");
        if (k & XINPUT_GAMEPAD_GUIDE) add("Guide");
        if (k & XINPUT_GAMEPAD_LEFT_SHOULDER) add("LB");
        if (k & XINPUT_GAMEPAD_RIGHT_SHOULDER) add("RB");
        if (k & XINPUT_GAMEPAD_A) add("A");
        if (k & XINPUT_GAMEPAD_B) add("B");
        if (k & XINPUT_GAMEPAD_X) add("X");
        if (k & XINPUT_GAMEPAD_Y) add("Y");
        
        if (first) str += "Unknown";
    }

    if (str.empty()) return "None";
    return str;
}

bool KeyBind::IsPressed(bool strict) const
{
    // Check Controller
    if (ControllerKey != 0)
    {
        XINPUT_STATE state{};
        // User index 0 is the primary controller
        if (GetControllerState(0, &state))
        {
            unsigned int currentButtons = GetGamepadFlags(state);
            
            // Check if ALL required bits are set
            if ((currentButtons & ControllerKey) == ControllerKey)
                return true;
        }
    }

    // Check Keyboard
    if (KeyboardKey != 0)
    {
        // Short-circuit: only check modifiers if the main key is physically pressed
        if (GetAsyncKeyState(KeyboardKey) & 0x8000)
        {
            const bool c = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
            const bool s = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
            const bool a = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;

            if (strict)
            {
                if ((Ctrl == c) && (Shift == s) && (Alt == a))
                    return true;
            }
            else
            {
                if ((!Ctrl || c) && (!Shift || s) && (!Alt || a))
                    return true;
            }
        }
    }

    return false;
}

bool KeyBind::IsPressedEvent(unsigned int msg, uintptr_t wParam, bool strict) const
{
    if (KeyboardKey == 0) return false;

    if (msg != WM_KEYDOWN && msg != WM_SYSKEYDOWN) return false;
    if (wParam != KeyboardKey) return false;

    bool c = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    bool s = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    bool a = (GetKeyState(VK_MENU) & 0x8000) != 0;

    if (strict)
        return (Ctrl == c) && (Shift == s) && (Alt == a);
    else
        return (!Ctrl || c) && (!Shift || s) && (!Alt || a);
}
