#include "pch.h"
#include "base.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace BaseHook
{
    namespace Hooks
    {
        LRESULT CALLBACK WndProc_Base(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
        {
            auto CallOriginal = [&](HWND h, UINT m, WPARAM w, LPARAM l) -> LRESULT {
                return Data::oWndProc ? CallWindowProc(Data::oWndProc, h, m, w, l) : DefWindowProc(h, m, w, l);
            };

            if (Data::bIsDetached)
                return CallOriginal(hWnd, uMsg, wParam, lParam);

            // 1. Internal Hotkeys
            if (uMsg == WM_KEYDOWN || uMsg == WM_SYSKEYDOWN)
            {
                if (wParam == Keys::ToggleMenu)
                {
                    Data::bShowMenu = !Data::bShowMenu;
                    Data::bBlockInput = Data::bShowMenu;
                }
                else if (wParam == Keys::DetachDll)
                {
                    Detach();
                    return CallOriginal(hWnd, uMsg, wParam, lParam);
                }
            }

            if (uMsg == WM_DEVICECHANGE)
            {
                Hooks::HandleDeviceChange(wParam, lParam);
            }

            if (Data::bIsInitialized && !Data::bIsDetached)
            {
                // 2. Always feed ImGui
                ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);

                // 3. Input Blocking Logic
                // If bBlockInput is TRUE (Menu Open), we aggressively block game input.
                // If bBlockInput is FALSE, we only block if ImGui specifically wants it.
                bool shouldBlock = Data::bBlockInput;

                if (!shouldBlock)
                {
                    ImGuiIO& io = ImGui::GetIO();
                    if (io.WantCaptureKeyboard && (uMsg == WM_KEYDOWN || uMsg == WM_KEYUP || uMsg == WM_CHAR || uMsg == WM_SYSKEYDOWN))
                        shouldBlock = true;
                    if (io.WantCaptureMouse && (uMsg >= WM_MOUSEFIRST && uMsg <= WM_MOUSELAST))
                        shouldBlock = true;
                }

                if (shouldBlock)
                {
                    switch (uMsg)
                    {
                    case WM_SYSKEYDOWN:
                    case WM_SYSKEYUP:
                        if (wParam == VK_RETURN && (lParam & (1 << 29))) return CallOriginal(hWnd, uMsg, wParam, lParam); // Alt+Enter
                    case WM_KEYDOWN: case WM_KEYUP:
                    case WM_CHAR: case WM_IME_CHAR:
                    case WM_MOUSEMOVE:
                    case WM_LBUTTONDOWN: case WM_LBUTTONUP:
                    case WM_RBUTTONDOWN: case WM_RBUTTONUP:
                    case WM_MBUTTONDOWN: case WM_MBUTTONUP:
                    case WM_XBUTTONDOWN: case WM_XBUTTONUP:
                    case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL:
                    case WM_INPUT: // Block Raw Input too
                        return 1; // Swallow message
                    }
                }
            }

            return CallOriginal(hWnd, uMsg, wParam, lParam);
        }
    }
}