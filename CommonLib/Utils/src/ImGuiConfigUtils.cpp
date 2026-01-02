#include "ImGuiConfigUtils.h"
#include "imgui_internal.h"
#include "KeyBind.h"
#include <Windows.h>
#include <xinput.h>
#include <vector>
#include <cstring>
#include <string>

namespace ImGui {

    bool KeyBindInput(const char* label, KeyBind& keybind)
    {
        bool changed = false;
        std::string labelStr = label;
        std::string idStr = "##" + labelStr;
        ImGuiID id = ImGui::GetID(idStr.c_str());

        // Shared state for live preview during capture
        static unsigned int s_accumulatedButtons = 0;
        static bool s_capturingPad = false;
        static ImGuiID s_currentActiveID = 0;

        bool is_active = (ImGui::GetActiveID() == id);

        // Reset state if we lost focus or switched widgets unexpectedly
        if (s_currentActiveID == id && !is_active)
        {
            s_currentActiveID = 0;
            s_accumulatedButtons = 0;
            s_capturingPad = false;
        }
        else if (is_active && s_currentActiveID != id)
        {
            s_currentActiveID = id;
            s_accumulatedButtons = 0;
            s_capturingPad = false;
        }

        // Determine display string for preview
        std::string valueStr;
        if (is_active && s_capturingPad && s_accumulatedButtons != 0)
        {
            // Create a temp object with current real KB bind + accumulated Pad bind for visualization
            KeyBind temp = keybind;
            temp.ControllerKey = s_accumulatedButtons;
            valueStr = temp.ToString();
        }
        else
        {
            valueStr = keybind.ToString();
        }

        char buf[128];
        // Use standard copy to avoid MSVC specific warnings if compiling elsewhere, but keep simple
        strncpy_s(buf, valueStr.c_str(), _TRUNCATE);

        // Pre-emptive blocking of Gamepad Navigation
        // If we are currently active (based on previous frame), we block ImGui from seeing gamepad keys
        // so it doesn't navigate away or consume 'A'/'B'.
        if (s_currentActiveID == id)
        {
            ImGuiID blockerId = ImGui::GetID((idStr + "_Blocker").c_str());
            for (int key = ImGuiKey_Gamepad_BEGIN; key < ImGuiKey_Gamepad_END; key++)
            {
                ImGui::SetKeyOwner((ImGuiKey)key, blockerId, ImGuiInputFlags_LockThisFrame);
            }
        }

        // Read-only input box. Used to capture focus and display text.
        ImGui::InputText(idStr.c_str(), buf, sizeof(buf), ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_NoUndoRedo);
        
        // Tooltip
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
        {
            ImGui::SetTooltip("Click to set shortcut.\nKeyboard: Press key to set.\nController: Hold buttons to set (supports combos).\nPress Backspace to clear all.");
        }

        // Capture Logic
        if (ImGui::IsItemActive())
        {
            // 1. Controller Poll
            XINPUT_STATE xState{};
            if (KeyBind::GetControllerState(0, &xState))
            {
                unsigned int currentButtons = KeyBind::GetGamepadFlags(xState);

                if (currentButtons != 0)
                {
                    s_capturingPad = true;
                    s_accumulatedButtons |= currentButtons;
                }
                else if (s_capturingPad) // Buttons released
                {
                    // Apply accumulated buttons
                    if (s_accumulatedButtons != 0)
                    {
                        keybind.ControllerKey = s_accumulatedButtons;
                        changed = true;
                        ImGui::ClearActiveID(); // Stop capturing
                    }
                    s_capturingPad = false;
                    s_accumulatedButtons = 0;
                }
            }

            // 2. Keyboard Poll
            // Only poll keyboard if we aren't currently holding controller buttons to avoid confusing conflicts
            if (!changed && !s_capturingPad)
            {
                // We use GetAsyncKeyState loop to detect key press
                // We scan range 0x08 (Backspace) to 0xFE
                unsigned int pressedKey = 0;
                
                for (unsigned int k = 0x08; k <= 0xFE; ++k)
                {
                    // Key must be pressed
                    if (GetAsyncKeyState(k) & 0x8000)
                    {
                        // Filter modifiers (Ctrl, Shift, Alt, Win) from being the "Main" key 
                        if (k == VK_SHIFT || k == VK_CONTROL || k == VK_MENU || k == VK_LWIN || k == VK_RWIN ||
                            k == VK_LSHIFT || k == VK_RSHIFT || k == VK_LCONTROL || k == VK_RCONTROL || k == VK_LMENU || k == VK_RMENU)
                            continue;
                        
                        // Avoid capturing mouse clicks as keys
                        if (k == VK_LBUTTON || k == VK_RBUTTON || k == VK_MBUTTON || k == VK_XBUTTON1 || k == VK_XBUTTON2) continue;

                        pressedKey = k;
                        break;
                    }
                }

                if (pressedKey != 0)
                {
                    if (pressedKey == VK_BACK)
                    {
                        // Backspace: Clear ALL Binds
                        keybind.KeyboardKey = 0;
                        keybind.Ctrl = false;
                        keybind.Shift = false;
                        keybind.Alt = false;
                        keybind.ControllerKey = 0;
                        changed = true;
                    }
                    else
                    {
                        // Set new Keyboard bind
                        keybind.KeyboardKey = pressedKey;
                        keybind.Ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
                        keybind.Shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
                        keybind.Alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
                        changed = true;
                    }
                    
                    ImGui::ClearActiveID();
                }
            }
        }
        
        ImGui::SameLine();
        ImGui::Text("%s", label);

        return changed;
    }
}
