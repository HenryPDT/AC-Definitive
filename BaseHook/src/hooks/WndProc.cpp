#include "pch.h"
#include "core/BaseHook.h"
#include "core/WindowedMode.h"

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

            // Scale mouse coordinates from Physical Window to Virtual Resolution (if applicable)
            // This ensures ImGui and the Game receive coordinates matching the render resolution.
            lParam = WindowedMode::ScaleMouseMessage(hWnd, uMsg, lParam);

            // Maintain windowed mode state on activation/focus changes
            if (uMsg == WM_ACTIVATE && LOWORD(wParam) != WA_INACTIVE)
            {
                if (WindowedMode::ShouldHandle())
                {
                    // Ensure styles are still correct when regaining focus
                    WindowedMode::Apply(hWnd);
                }
                else
                {
                    // Request restoration of exclusive fullscreen if configured
                    WindowedMode::RequestRestoreExclusiveFullscreen();
                }
            }

            if (WindowedMode::ShouldHandle())
            {
                switch (uMsg)
                {
                case WM_SIZE:
                    // Spoof the size to match Virtual Resolution.
                    // This ensures the game "sees" the constant resolution we want it to run at,
                    // regardless of the actual physical window dimensions (e.g. if resizing is allowed).
                    if (WindowedMode::g_State.virtualWidth > 0 && WindowedMode::g_State.virtualHeight > 0)
                    {
                         lParam = MAKELPARAM(WindowedMode::g_State.virtualWidth, WindowedMode::g_State.virtualHeight);
                    }
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

            if (uMsg == WM_DISPLAYCHANGE)
            {
                WindowedMode::g_State.needMonitorRefresh = true;
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