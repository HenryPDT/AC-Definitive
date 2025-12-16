#include "ImGuiConfigUtils.h"
#include "imgui_internal.h"
#include "KeyBind.h"
#include <Windows.h>
#include <vector>
#include <cstring>

namespace ImGui {

    bool KeyBindInput(const char* label, KeyBind& keybind)
    {
        bool changed = false;
        std::string labelStr = label;
        std::string idStr = "##" + labelStr;
        
        // Display current binding
        std::string valueStr = keybind.ToString();
        char buf[128];
        strcpy_s(buf, valueStr.c_str());

        // Read-only input box. Used to capture focus.
        ImGui::InputText(idStr.c_str(), buf, sizeof(buf), ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_NoUndoRedo);
        
        // Tooltip
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
        {
            ImGui::SetTooltip("Click to set shortcut.\nPress Backspace to clear.");
        }

        // Capture Logic
        if (ImGui::IsItemActive())
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
                    // unless we want to allow binding just "Ctrl". ReShade blocks them.
                    // 0x10=Shift, 0x11=Ctrl, 0x12=Alt, 0x5B/5C=Win
                    if (k == VK_SHIFT || k == VK_CONTROL || k == VK_MENU || k == VK_LWIN || k == VK_RWIN ||
                        k == VK_LSHIFT || k == VK_RSHIFT || k == VK_LCONTROL || k == VK_RCONTROL || k == VK_LMENU || k == VK_RMENU)
                        continue;
                    
                    // Mouse buttons (LButton=1, RButton=2...) 
                    // We typically avoid binding LButton (0x01) directly via this loop 
                    // because clicking the widget triggers it.
                    if (k == VK_LBUTTON) continue;

                    pressedKey = k;
                    break;
                }
            }

            if (pressedKey != 0)
            {
                if (pressedKey == VK_BACK)
                {
                    // Clear
                    keybind = KeyBind();
                    changed = true;
                }
                else
                {
                    // Set new bind
                    keybind.Key = pressedKey;
                    keybind.Ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
                    keybind.Shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
                    keybind.Alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
                    changed = true;
                }
                
                // Remove focus to stop capturing
                ImGui::ClearActiveID();
            }
        }
        
        ImGui::SameLine();
        ImGui::Text("%s", label);

        return changed;
    }
}