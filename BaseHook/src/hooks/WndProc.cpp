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

            // Handle window resizing to update virtual monitor bounds logic
            if (uMsg == WM_SIZE && hWnd == Data::hWindow)
            {
                int width = LOWORD(lParam);
                int height = HIWORD(lParam);
                WindowedMode::NotifyWindowResize(width, height);
            }

            // [FIX] Force Capture for ImGui Viewport Dragging
            // We do this globally here because it must happen regardless of whether input
            // is fed to ImGui via Win32 messages or DirectInput.
            if (uMsg == WM_LBUTTONDOWN)
            {
                const bool wantCapture = (ImGui::GetCurrentContext() != nullptr) && ImGui::GetIO().WantCaptureMouse;
                const bool blockInput = Data::bBlockInput;

                if ((wantCapture || blockInput) && ::GetCapture() == NULL) {
                    ::SetCapture(hWnd);
                }
            }
            else if (uMsg == WM_LBUTTONUP) {
                if (::GetCapture() == hWnd) {
                    ::ReleaseCapture();
                }
            }

            // 1. Feed ImGui before any potential blocking
            if (Data::bIsInitialized && !Data::bIsDetached)
            {
                // We filter mouse buttons/wheel if DirectInput is configured to drive them for ImGui.
                bool isMouseBtn = (uMsg == WM_LBUTTONDOWN || uMsg == WM_LBUTTONUP || uMsg == WM_RBUTTONDOWN || uMsg == WM_RBUTTONUP ||
                                   uMsg == WM_MBUTTONDOWN || uMsg == WM_MBUTTONUP || uMsg == WM_XBUTTONDOWN || uMsg == WM_XBUTTONUP ||
                                   uMsg == WM_MOUSEWHEEL || uMsg == WM_MOUSEHWHEEL);

                if (!Data::bImGuiMouseButtonsFromDirectInput || !isMouseBtn)
                {
                    ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
                }
            }

            // 2. Block pure input messages if menu/overlay is open, but NEVER block window management.
            if (Data::bBlockInput && !Data::bIsDetached)
            {
                switch (uMsg)
                {
                case WM_SYSKEYDOWN:
                case WM_SYSKEYUP:
                    if (wParam == VK_F4) break; // Always allow Alt+F4
                    return 1;
                case WM_KEYDOWN:
                case WM_KEYUP:
                case WM_CHAR:
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
                default:
                    break;
                }
            }

            // 3. Handle specific spoofing/lifecycle tasks
            if (uMsg == WM_ACTIVATE || uMsg == WM_ACTIVATEAPP)
            {
                HWND hWndGainingFocus = (uMsg == WM_ACTIVATE) ? (HWND)lParam : NULL;
                bool toImGui = (uMsg == WM_ACTIVATE && WindowedMode::IsImGuiPlatformWindow(hWndGainingFocus));

                if (toImGui || (Data::pSettings && Data::pSettings->m_RenderInBackground))
                {
                    if ((uMsg == WM_ACTIVATE && LOWORD(wParam) == WA_INACTIVE) || (uMsg == WM_ACTIVATEAPP && wParam == FALSE))
                    {
                        bool stayActive = toImGui;
                        if (!stayActive) {
                            HWND fg = GetForegroundWindow();
                            if (fg) {
                                DWORD pid;
                                GetWindowThreadProcessId(fg, &pid);
                                if (pid == GetCurrentProcessId()) stayActive = true;
                            }
                        }

                        if (stayActive) {
                            if (uMsg == WM_ACTIVATE) wParam = WA_ACTIVE;
                            else wParam = TRUE;
                        }
                    }
                }

                if ((uMsg == WM_ACTIVATE && LOWORD(wParam) != WA_INACTIVE) || (uMsg == WM_ACTIVATEAPP && wParam != FALSE))
                {
                    if (WindowedMode::ShouldHandle()) WindowedMode::Apply(hWnd);
                    else WindowedMode::RequestRestoreExclusiveFullscreen();
                }
            }

            if (WindowedMode::ShouldHandle() && uMsg == WM_SIZE)
            {
                if (WindowedMode::g_State.virtualWidth > 0 && WindowedMode::g_State.virtualHeight > 0)
                {
                    lParam = MAKELPARAM(WindowedMode::g_State.virtualWidth, WindowedMode::g_State.virtualHeight);
                }
            }

            if (uMsg == WM_KEYDOWN || uMsg == WM_SYSKEYDOWN)
            {
                if (wParam == Keys::DetachDll)
                {
                    Detach();
                    return CallOriginal(hWnd, uMsg, wParam, lParam);
                }
            }

            if (uMsg == WM_DEVICECHANGE) Hooks::HandleDeviceChange(wParam, lParam);
            if (uMsg == WM_DISPLAYCHANGE) WindowedMode::g_State.needMonitorRefresh = true;

            // Track title bar dragging to sync window position after user moves window
            if (uMsg == WM_ENTERSIZEMOVE && hWnd == Data::hWindow)
            {
                WindowedMode::g_State.isSystemMoving = true;
            }
            else if (uMsg == WM_EXITSIZEMOVE && hWnd == Data::hWindow)
            {
                if (WindowedMode::g_State.isSystemMoving)
                {
                    WindowedMode::SyncStateFromWindow();
                }
                WindowedMode::g_State.isSystemMoving = false;
            }

            // Only refresh platform windows for main window events
            // Secondary viewports are handled by ImGui's native platform backend
            if (WindowedMode::IsMultiViewportEnabled() && hWnd == Data::hWindow)
            {
                if (uMsg == WM_EXITSIZEMOVE)
                {
                    // Main window finished moving/resizing - sync all viewport positions
                    WindowedMode::RefreshPlatformWindows();
                }
                else if (uMsg == WM_WINDOWPOSCHANGED)
                {
                    // Main window position changed - update virtual coordinates
                    WindowedMode::RefreshPlatformWindows();
                }
            }

            return CallOriginal(hWnd, uMsg, wParam, lParam);
        }
    }
}