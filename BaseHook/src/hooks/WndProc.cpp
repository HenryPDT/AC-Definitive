#include "pch.h"
#include "base.h"
#include "WindowedMode.h"

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

            // Maintain windowed mode state on activation/focus changes
            if (WindowedMode::ShouldHandle())
            {
                switch (uMsg)
                {
                case WM_ACTIVATE:
                    if (LOWORD(wParam) != WA_INACTIVE)
                    {
                        // Ensure styles are still correct when regaining focus
                        WindowedMode::Apply(hWnd);
                    }
                    break;
                case WM_SIZE:
                    // We generally ignore game resize requests to physical window
                    break;
                }
            }
            if (uMsg == WM_KEYDOWN || uMsg == WM_SYSKEYDOWN)
            {
                // Menu toggle is handled by PluginLoader (configurable). BaseHook should not own UI hotkeys.
                if (wParam == Keys::DetachDll)
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
                // Plumbing: feed ImGui's Win32 backend so it can keep internal state coherent.
                ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
            }

            return CallOriginal(hWnd, uMsg, wParam, lParam);
        }
    }
}