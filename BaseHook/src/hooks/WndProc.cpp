#include "pch.h"
#include "base.h"

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace BaseHook
{
    namespace Hooks
    {
        LRESULT CALLBACK WndProc_Base(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
        {
            // Handle our own hotkeys first.
            if (uMsg == WM_KEYDOWN)
            {
                if (wParam == Keys::ToggleMenu)
                {
                    Data::bShowMenu = !Data::bShowMenu;
                }
                else if (wParam == Keys::DetachDll)
                {
                    Detach();
                    return CallWindowProc(Data::oWndProc, hWnd, uMsg, wParam, lParam);
                }
            }

            // Always feed messages to ImGui so it can update its internal state.
            if (Data::bIsInitialized && !Data::bIsDetached)
            {
                ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);

                // Only when our menu is open do we want to stop the game from seeing
                // keyboard/mouse input. This avoids swallowing window-management
                // messages the game needs for fullscreen/Alt+Tab/device handling.
                if (Data::bShowMenu)
                {
                    ImGuiIO& io = ImGui::GetIO();
                    const bool wantCaptureKeyboard = io.WantCaptureKeyboard || io.WantTextInput;
                    const bool wantCaptureMouse    = io.WantCaptureMouse;

                    if (wantCaptureKeyboard || wantCaptureMouse)
                    {
                        switch (uMsg)
                        {
                        // Keyboard input
                        case WM_KEYDOWN:
                        case WM_KEYUP:
                        case WM_SYSKEYDOWN:
                        case WM_SYSKEYUP:
                        case WM_CHAR:
                            return 1;

                        // Mouse input
                        case WM_MOUSEMOVE:
                        case WM_LBUTTONDOWN:
                        case WM_LBUTTONUP:
                        case WM_RBUTTONDOWN:
                        case WM_RBUTTONUP:
                        case WM_MBUTTONDOWN:
                        case WM_MBUTTONUP:
                        case WM_XBUTTONDOWN:
                        case WM_XBUTTONUP:
                        case WM_MOUSEWHEEL:
                        case WM_MOUSEHWHEEL:
                            return 1;

                        // For everything else (activation, sizing, system commands, etc.)
                        // fall through and let the game handle it to keep Alt+Tab and
                        // fullscreen-exclusive behaviour intact.
                        default:
                            break;
                        }
                    }
                }
            }

            return CallWindowProc(Data::oWndProc, hWnd, uMsg, wParam, lParam);
        }
    }
}