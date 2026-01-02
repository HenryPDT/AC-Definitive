#include "pch.h"
#include "base.h"
#include "WindowedMode.h"
#include "hooks/hooks.h"
#include "hooks/D3DCreateHooks.h"
#include <vector>
#include <string>

// Separate implementation file for Windowed Mode Hooks (User32, D3D Creation, Input)
// This keeps hooks.cpp clean and focused on lifecycle management.

namespace BaseHook
{
    namespace Hooks
    {
        // --- Function Typedefs ---
        typedef HWND(WINAPI* CreateWindowExA_t)(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID);
        typedef HWND(WINAPI* CreateWindowExW_t)(DWORD, LPCWSTR, LPCWSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID);
        typedef BOOL(WINAPI* SetWindowPos_t)(HWND, HWND, int, int, int, int, UINT);
        typedef BOOL(WINAPI* MoveWindow_t)(HWND, int, int, int, int, BOOL);
        typedef LONG(WINAPI* GetWindowLongA_t)(HWND, int);
        typedef LONG(WINAPI* GetWindowLongW_t)(HWND, int);
        typedef LONG_PTR(WINAPI* GetWindowLongPtrA_t)(HWND, int);
        typedef LONG_PTR(WINAPI* GetWindowLongPtrW_t)(HWND, int);
        typedef LONG(WINAPI* SetWindowLongA_t)(HWND, int, LONG);
        typedef LONG(WINAPI* SetWindowLongW_t)(HWND, int, LONG);
        typedef LONG_PTR(WINAPI* SetWindowLongPtrA_t)(HWND, int, LONG_PTR);
        typedef LONG_PTR(WINAPI* SetWindowLongPtrW_t)(HWND, int, LONG_PTR);
        typedef LONG(WINAPI* ChangeDisplaySettingsA_t)(DEVMODEA*, DWORD);
        typedef LONG(WINAPI* ChangeDisplaySettingsW_t)(DEVMODEW*, DWORD);
        typedef LONG(WINAPI* ChangeDisplaySettingsExA_t)(LPCSTR, DEVMODEA*, HWND, DWORD, LPVOID);
        typedef LONG(WINAPI* ChangeDisplaySettingsExW_t)(LPCWSTR, DEVMODEW*, HWND, DWORD, LPVOID);
        typedef BOOL(WINAPI* EnumDisplaySettingsA_t)(LPCSTR, DWORD, DEVMODEA*);
        typedef BOOL(WINAPI* EnumDisplaySettingsW_t)(LPCWSTR, DWORD, DEVMODEW*);
        typedef BOOL(WINAPI* GetCursorPos_t)(LPPOINT);
        typedef BOOL(WINAPI* SetCursorPos_t)(int, int);
        typedef BOOL(WINAPI* ClipCursor_t)(const RECT*);
        typedef BOOL(WINAPI* SetWindowPlacement_t)(HWND, const WINDOWPLACEMENT*);

        // --- Original Pointers ---
        CreateWindowExA_t oCreateWindowExA = nullptr;
        CreateWindowExW_t oCreateWindowExW = nullptr;
        SetWindowPos_t oSetWindowPos = nullptr;
        MoveWindow_t oMoveWindow = nullptr;
        GetWindowLongA_t oGetWindowLongA = nullptr;
        GetWindowLongW_t oGetWindowLongW = nullptr;
        GetWindowLongPtrA_t oGetWindowLongPtrA = nullptr;
        GetWindowLongPtrW_t oGetWindowLongPtrW = nullptr;
        SetWindowLongA_t oSetWindowLongA = nullptr;
        SetWindowLongW_t oSetWindowLongW = nullptr;
        SetWindowLongPtrA_t oSetWindowLongPtrA = nullptr;
        SetWindowLongPtrW_t oSetWindowLongPtrW = nullptr;
        ChangeDisplaySettingsA_t oChangeDisplaySettingsA = nullptr;
        ChangeDisplaySettingsW_t oChangeDisplaySettingsW = nullptr;
        ChangeDisplaySettingsExA_t oChangeDisplaySettingsExA = nullptr;
        ChangeDisplaySettingsExW_t oChangeDisplaySettingsExW = nullptr;
        EnumDisplaySettingsA_t oEnumDisplaySettingsA = nullptr;
        EnumDisplaySettingsW_t oEnumDisplaySettingsW = nullptr;
        GetCursorPos_t oGetCursorPos = nullptr;
        SetCursorPos_t oSetCursorPos = nullptr;
        ClipCursor_t oClipCursor = nullptr;
        SetWindowPlacement_t oSetWindowPlacement = nullptr;

        // (D3D/DXGI creation hooks moved to hooks/D3DCreateHooks.cpp)

        // --- Helper Logic ---
        
        DWORD GetTrueWindowStyle(HWND hWnd)
        {
            if (IsWindowUnicode(hWnd))
                return oGetWindowLongW ? (DWORD)oGetWindowLongW(hWnd, GWL_STYLE) : (DWORD)GetWindowLongW(hWnd, GWL_STYLE);
            else
                return oGetWindowLongA ? (DWORD)oGetWindowLongA(hWnd, GWL_STYLE) : (DWORD)GetWindowLongA(hWnd, GWL_STYLE);
        }

        DWORD GetTrueWindowExStyle(HWND hWnd)
        {
            if (IsWindowUnicode(hWnd))
                return oGetWindowLongW ? (DWORD)oGetWindowLongW(hWnd, GWL_EXSTYLE) : (DWORD)GetWindowLongW(hWnd, GWL_EXSTYLE);
            else
                return oGetWindowLongA ? (DWORD)oGetWindowLongA(hWnd, GWL_EXSTYLE) : (DWORD)GetWindowLongA(hWnd, GWL_EXSTYLE);
        }

        static std::atomic<bool> g_WindowFound = false;
        static long g_MaxArea = 0;

        template <typename CharT>
        bool IsIgnoredWindowClass(const CharT* lpClassName, const CharT* lpWindowName, int nWidth, int nHeight)
        {
            // Ignore tiny/hidden windows
            if (nWidth != (int)0x80000000 && nWidth <= 32 && nWidth >= 0) {
                 // Too spammy?
                 // LOG_INFO("IsIgnoredWindowClass: Ignored tiny window (%dx%d)", nWidth, nHeight);
                 return true;
            }
            if (nHeight != (int)0x80000000 && nHeight <= 32 && nHeight >= 0) return true;

            // Helper to check string contains
            auto contains = [](const CharT* haystack, const CharT* needle) {
                if constexpr (std::is_same_v<CharT, char>) return strstr(haystack, needle) != nullptr;
                else return wcsstr(haystack, needle) != nullptr;
            };
            
            auto equals = [](const CharT* a, const CharT* b) {
                 if constexpr (std::is_same_v<CharT, char>) return strcmp(a, b) == 0;
                 else return wcscmp(a, b) == 0;
            };

            // Class Name Check
            if (lpClassName && !IS_INTRESOURCE(lpClassName)) {
                if constexpr (std::is_same_v<CharT, char>) {
                    if (equals(lpClassName, "ConsoleWindowClass")) return true;
                    if (contains(lpClassName, "DIEmWin")) return true;
                    if (contains(lpClassName, "IME")) return true;
                    if (contains(lpClassName, "CTF")) return true;
                    if (contains(lpClassName, "CicMarshalWnd")) return true;
                    if (equals(lpClassName, "DummyWindow")) return true; // Ignore internal dummy windows
                    if (contains(lpClassName, "D3DProxyWindow")) {
                        LOG_INFO("IsIgnoredWindowClass: Ignored D3DProxyWindow");
                        return true;
                    }
                }
                else {
                    if (equals(lpClassName, L"ConsoleWindowClass")) return true;
                    if (contains(lpClassName, L"DIEmWin")) return true;
                    if (contains(lpClassName, L"IME")) return true;
                    if (contains(lpClassName, L"CTF")) return true;
                    if (contains(lpClassName, L"CicMarshalWnd")) return true;
                    if (equals(lpClassName, L"DummyWindow")) return true;
                    if (contains(lpClassName, L"D3DProxyWindow")) {
                        // LOG_INFO("IsIgnoredWindowClass: Ignored D3DProxyWindow (Wide)");
                        return true;
                    }
                }
            }

            // Window Name Check
            if (lpWindowName && !IS_INTRESOURCE(lpWindowName)) {
                if constexpr (std::is_same_v<CharT, char>) {
                    if (contains(lpWindowName, "DIEmWin")) return true;
                    if (contains(lpWindowName, "Default IME")) return true;
                    if (equals(lpWindowName, "Kiero DirectX Window")) {
                        LOG_INFO("IsIgnoredWindowClass: Ignored Kiero Window");
                        return true;
                    }
                }
                else {
                    if (contains(lpWindowName, L"DIEmWin")) return true;
                    if (contains(lpWindowName, L"Default IME")) return true;
                    if (equals(lpWindowName, L"Kiero DirectX Window")) {
                        return true;
                    }
                }
            }

            return false;
        }

        template <typename CharT>
        void ApplyWindowCreationOverrides(const CharT* className, const CharT* windowName, DWORD& dwStyle, DWORD& dwExStyle, int& X, int& Y, int& nWidth, int& nHeight)
        {
            // Sanitize Styles
            if (WindowedMode::g_State.activeMode == WindowedMode::Mode::BorderlessFullscreen || WindowedMode::g_State.activeMode == WindowedMode::Mode::Borderless) {
                dwStyle &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
                dwStyle |= WS_POPUP;
            }
            else if (WindowedMode::g_State.activeMode == WindowedMode::Mode::Bordered) {
                dwStyle |= WS_OVERLAPPEDWINDOW;
                dwStyle &= ~WS_POPUP;
            }

            if (WindowedMode::g_State.alwaysOnTop)
                dwExStyle |= WS_EX_TOPMOST;
            else
                dwExStyle &= ~WS_EX_TOPMOST;

            // Calculate Dimensions
            int w = WindowedMode::g_State.windowWidth;
            int h = WindowedMode::g_State.windowHeight;
            int x = WindowedMode::g_State.windowX;
            int y = WindowedMode::g_State.windowY;

            int screenW = Data::oGetSystemMetrics ? ((GetSystemMetrics_t)Data::oGetSystemMetrics)(SM_CXSCREEN) : GetSystemMetrics(SM_CXSCREEN);
            int screenH = Data::oGetSystemMetrics ? ((GetSystemMetrics_t)Data::oGetSystemMetrics)(SM_CYSCREEN) : GetSystemMetrics(SM_CYSCREEN);

            if (WindowedMode::g_State.activeMode == WindowedMode::Mode::BorderlessFullscreen) {
                // For multi-monitor: choose the target monitor and use its origin (virtual screen coords).
                // Use the incoming CreateWindow position if it's explicit; otherwise fall back to config.
                int hintX = (X != CW_USEDEFAULT && X != (int)0x80000000) ? X : x;
                int hintY = (Y != CW_USEDEFAULT && Y != (int)0x80000000) ? Y : y;
                if (hintX == -1) hintX = 0;
                if (hintY == -1) hintY = 0;

                const HMONITOR mon = MonitorFromPoint(POINT{ hintX, hintY }, MONITOR_DEFAULTTOPRIMARY);
                MONITORINFOEXA mi{};
                mi.cbSize = sizeof(mi);
                if (mon && GetMonitorInfoA(mon, &mi))
                {
                    const int monW = (int)(mi.rcMonitor.right - mi.rcMonitor.left);
                    const int monH = (int)(mi.rcMonitor.bottom - mi.rcMonitor.top);

                    int deskW = monW;
                    int deskH = monH;

                    DEVMODEA dm{};
                    dm.dmSize = sizeof(dm);
                    if (EnumDisplaySettingsExA(mi.szDevice, ENUM_REGISTRY_SETTINGS, &dm, 0))
                    {
                        deskW = (int)dm.dmPelsWidth;
                        deskH = (int)dm.dmPelsHeight;
                    }

                    w = (deskW > 0 ? deskW : monW);
                    h = (deskH > 0 ? deskH : monH);
                    x = (int)mi.rcMonitor.left;
                    y = (int)mi.rcMonitor.top;
                }
                else
                {
                    WindowedMode::GetFixedDesktopResolution(w, h);
                    x = 0; y = 0;
                }
            }
            else {
                RECT rect = { 0, 0, w, h };
                if (Data::oAdjustWindowRectEx) ((AdjustWindowRectEx_t)Data::oAdjustWindowRectEx)(&rect, dwStyle, FALSE, dwExStyle);
                else AdjustWindowRectEx(&rect, dwStyle, FALSE, dwExStyle);

                int fullW = rect.right - rect.left;
                int fullH = rect.bottom - rect.top;

                if (x == -1) x = (screenW - fullW) / 2;
                if (y == -1) y = (screenH - fullH) / 2;
                w = fullW; h = fullH;
            }
            X = x; Y = y; nWidth = w; nHeight = h;
        }

        // --- Hook Implementations ---

        BOOL WINAPI hkScreenToClient(HWND hWnd, LPPOINT lpPoint)
        {
            BOOL res = ((ScreenToClient_t)Data::oScreenToClient)(hWnd, lpPoint);
            if (res)
                WindowedMode::PhysicalClientToVirtual(hWnd, *lpPoint);
            return res;
        }

        BOOL WINAPI hkClientToScreen(HWND hWnd, LPPOINT lpPoint)
        {
            WindowedMode::VirtualClientToPhysical(hWnd, *lpPoint);
            return ((ClientToScreen_t)Data::oClientToScreen)(hWnd, lpPoint);
        }

        BOOL WINAPI hkGetClientRect(HWND hWnd, LPRECT lpRect)
        {
            if (WindowedMode::ShouldHandle() && hWnd == Data::hWindow) {
                lpRect->left = 0;
                lpRect->top = 0;
                lpRect->right = WindowedMode::g_State.virtualWidth;
                lpRect->bottom = WindowedMode::g_State.virtualHeight;
                return TRUE;
            }
            return ((GetClientRect_t)Data::oGetClientRect)(hWnd, lpRect);
        }

        BOOL WINAPI hkGetWindowRect(HWND hWnd, LPRECT lpRect)
        {
            if (WindowedMode::ShouldHandle() && hWnd == Data::hWindow) {
                lpRect->left = 0;
                lpRect->top = 0;
                lpRect->right = WindowedMode::g_State.virtualWidth;
                lpRect->bottom = WindowedMode::g_State.virtualHeight;
                return TRUE;
            }
            return ((GetWindowRect_t)Data::oGetWindowRect)(hWnd, lpRect);
        }

        HWND WINAPI hkCreateWindowExA(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam)
        {
            if (!IsIgnoredWindowClass(lpClassName, lpWindowName, nWidth, nHeight)) {
                LOG_INFO("hkCreateWindowExA: Class=%s, Name=%s, Style=%08X, ExStyle=%08X, X=%d, Y=%d, W=%d, H=%d, Parent=%p",
                    (IS_INTRESOURCE(lpClassName) ? "INT" : lpClassName),
                    (IS_INTRESOURCE(lpWindowName) ? "INT" : lpWindowName),
                    dwStyle, dwExStyle, X, Y, nWidth, nHeight, hWndParent);
            }

            bool bIgnored = IsIgnoredWindowClass(lpClassName, lpWindowName, nWidth, nHeight);
            if (WindowedMode::ShouldHandle() && !hWndParent && !bIgnored) {
                ApplyWindowCreationOverrides(lpClassName, lpWindowName, dwStyle, dwExStyle, X, Y, nWidth, nHeight);
                LOG_INFO("  Overrides Applied: Style=%08X, ExStyle=%08X, X=%d, Y=%d, W=%d, H=%d", dwStyle, dwExStyle, X, Y, nWidth, nHeight);
            }

            HWND hWnd = oCreateWindowExA(dwExStyle, lpClassName, lpWindowName, dwStyle, X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);

            if (hWnd && !hWndParent && !bIgnored) {
                if (WindowedMode::ShouldHandle()) {
                    int dx = WindowedMode::g_State.targetDXVersion;
                    if (dx == 9) {
                        Hooks::InstallD3D9CreateHooksLate();
                    }
                    else if (dx == 10 || dx == 11) {
                        Hooks::InstallDXGIHooksLate();
                    }
                    WindowedMode::Apply(hWnd);
                }
                long area = nWidth * nHeight;
                if (!g_WindowFound || area > g_MaxArea) {
                    if (g_WindowFound) RestoreWndProc();
                    Data::hWindow = hWnd;
                    g_MaxArea = area;
                    g_WindowFound = true;
                    InstallWndProcHook();
                    LOG_INFO("  Window Found & Hooked: %p", hWnd);
                }
            }
            return hWnd;
        }

        HWND WINAPI hkCreateWindowExW(DWORD dwExStyle, LPCWSTR lpClassName, LPCWSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam)
        {
            if (!IsIgnoredWindowClass(lpClassName, lpWindowName, nWidth, nHeight)) {
                 // Convert for logging (lazy)
                 // LOG_INFO("hkCreateWindowExW: Called");
            }

            bool bIgnored = IsIgnoredWindowClass(lpClassName, lpWindowName, nWidth, nHeight);
            if (WindowedMode::ShouldHandle() && !hWndParent && !bIgnored) {
                ApplyWindowCreationOverrides(lpClassName, lpWindowName, dwStyle, dwExStyle, X, Y, nWidth, nHeight);
            }

            HWND hWnd = oCreateWindowExW(dwExStyle, lpClassName, lpWindowName, dwStyle, X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
            
            if (hWnd && !hWndParent && !bIgnored) {
                if (WindowedMode::ShouldHandle()) {
                    int dx = WindowedMode::g_State.targetDXVersion;
                    if (dx == 9) {
                        Hooks::InstallD3D9CreateHooksLate();
                    }
                    else if (dx == 10 || dx == 11) {
                        Hooks::InstallDXGIHooksLate();
                    }
                    WindowedMode::Apply(hWnd);
                }
                long area = nWidth * nHeight;
                if (!g_WindowFound || area > g_MaxArea) {
                    if (g_WindowFound) RestoreWndProc();
                    Data::hWindow = hWnd;
                    g_MaxArea = area;
                    g_WindowFound = true;
                    InstallWndProcHook();
                    LOG_INFO("hkCreateWindowExW: Window Found & Hooked: %p", hWnd);
                }
            }
            return hWnd;
        }

        BOOL WINAPI hkSetWindowPos(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags)
        {
            if (WindowedMode::ShouldHandle() && hWnd == Data::hWindow && !WindowedMode::g_State.inInternalChange) {
                LOG_INFO("hkSetWindowPos: Called for Game Window. X=%d, Y=%d, CX=%d, CY=%d, Flags=%08X, InsertAfter=%p", X, Y, cx, cy, uFlags, hWndInsertAfter);

                if (WindowedMode::g_State.alwaysOnTop) {
                    hWndInsertAfter = HWND_TOPMOST;
                }
                else if (hWndInsertAfter == HWND_TOPMOST) {
                    hWndInsertAfter = HWND_NOTOPMOST;
                }

                // Enforce Position/Size if game tries to change it
                // Logic:
                // 1. If ScaleContent, enforce fixed Size.
                // 2. Always enforce fixed Position (or Center) to prevent D3D from moving window to (0,0) on Reset.

                bool enforceSize = (WindowedMode::g_State.resizeBehavior == WindowedMode::ResizeBehavior::ScaleContent);
                bool enforcePos = true;

                if (enforceSize)
                {
                    if (!(uFlags & SWP_NOSIZE)) {
                        cx = WindowedMode::g_State.windowWidth;
                        cy = WindowedMode::g_State.windowHeight;
                        LOG_INFO("  Enforcing Size: %dx%d", cx, cy);
                    }
                }

                if (enforcePos && !(uFlags & SWP_NOMOVE))
                {
                    // We need to determine the effective width/height to calculate center
                    int w = cx;
                    int h = cy;
                    if (uFlags & SWP_NOSIZE) {
                        RECT rc;
                        if (Data::oGetWindowRect) ((GetWindowRect_t)Data::oGetWindowRect)(hWnd, &rc);
                        else GetWindowRect(hWnd, &rc);
                        w = rc.right - rc.left;
                        h = rc.bottom - rc.top;
                    }

                    int desiredX = WindowedMode::g_State.windowX;
                    int desiredY = WindowedMode::g_State.windowY;

                    // If set to Center (-1), calculate centered coordinates
                    if (desiredX == -1 || desiredY == -1) {
                        RECT monitorRect = { 0,0,0,0 };
                        const auto& mons = WindowedMode::GetMonitors();
                        if (WindowedMode::g_State.targetMonitor >= 0 && WindowedMode::g_State.targetMonitor < (int)mons.size()) {
                            monitorRect = mons[WindowedMode::g_State.targetMonitor].rect;
                        }
                        else {
                            int sW = Data::oGetSystemMetrics ? ((GetSystemMetrics_t)Data::oGetSystemMetrics)(SM_CXSCREEN) : GetSystemMetrics(SM_CXSCREEN);
                            int sH = Data::oGetSystemMetrics ? ((GetSystemMetrics_t)Data::oGetSystemMetrics)(SM_CYSCREEN) : GetSystemMetrics(SM_CYSCREEN);
                            monitorRect = { 0, 0, sW, sH };
                        }

                        if (desiredX == -1) desiredX = monitorRect.left + (monitorRect.right - monitorRect.left - w) / 2;
                        if (desiredY == -1) desiredY = monitorRect.top + (monitorRect.bottom - monitorRect.top - h) / 2;
                    }

                    X = desiredX;
                    Y = desiredY;
                    LOG_INFO("  Enforcing Pos: %d,%d", X, Y);
                }
            }
            return oSetWindowPos(hWnd, hWndInsertAfter, X, Y, cx, cy, uFlags);
        }

        BOOL WINAPI hkMoveWindow(HWND hWnd, int X, int Y, int nWidth, int nHeight, BOOL bRepaint)
        {
            if (WindowedMode::ShouldHandle() && hWnd == Data::hWindow && !WindowedMode::g_State.inInternalChange) {
                bool enforceSize = (WindowedMode::g_State.resizeBehavior == WindowedMode::ResizeBehavior::ScaleContent);
                bool enforcePos = true;

                if (enforceSize) {
                    nWidth = WindowedMode::g_State.windowWidth;
                    nHeight = WindowedMode::g_State.windowHeight;
                }

                if (enforcePos) {
                    int desiredX = WindowedMode::g_State.windowX;
                    int desiredY = WindowedMode::g_State.windowY;

                    if (desiredX == -1 || desiredY == -1) {
                        RECT monitorRect = { 0,0,0,0 };
                        const auto& mons = WindowedMode::GetMonitors();
                        if (WindowedMode::g_State.targetMonitor >= 0 && WindowedMode::g_State.targetMonitor < (int)mons.size()) {
                            monitorRect = mons[WindowedMode::g_State.targetMonitor].rect;
                        }
                        else {
                            int sW = Data::oGetSystemMetrics ? ((GetSystemMetrics_t)Data::oGetSystemMetrics)(SM_CXSCREEN) : GetSystemMetrics(SM_CXSCREEN);
                            int sH = Data::oGetSystemMetrics ? ((GetSystemMetrics_t)Data::oGetSystemMetrics)(SM_CYSCREEN) : GetSystemMetrics(SM_CYSCREEN);
                            monitorRect = { 0, 0, sW, sH };
                        }

                        if (desiredX == -1) desiredX = monitorRect.left + (monitorRect.right - monitorRect.left - nWidth) / 2;
                        if (desiredY == -1) desiredY = monitorRect.top + (monitorRect.bottom - monitorRect.top - nHeight) / 2;
                    }

                    X = desiredX;
                    Y = desiredY;
                }
            }
            return oMoveWindow(hWnd, X, Y, nWidth, nHeight, bRepaint);
        }

        template <typename T, typename Func>
        LONG WINAPI hkChangeDisplaySettings_Impl(T* lpDevMode, DWORD dwFlags, Func original, const char* name)
        {
            if (WindowedMode::ShouldHandle() && !WindowedMode::g_State.inInternalChange) {
                if (lpDevMode) {
                    LOG_INFO("%s Spoof: %dx%d", name, lpDevMode->dmPelsWidth, lpDevMode->dmPelsHeight);
                    WindowedMode::NotifyResolutionChange(lpDevMode->dmPelsWidth, lpDevMode->dmPelsHeight);
                }
                return DISP_CHANGE_SUCCESSFUL;
            }
            return original(lpDevMode, dwFlags);
        }

        LONG WINAPI hkChangeDisplaySettingsA(DEVMODEA* lpDevMode, DWORD dwFlags) { return hkChangeDisplaySettings_Impl(lpDevMode, dwFlags, oChangeDisplaySettingsA, "ChangeDisplaySettingsA"); }
        LONG WINAPI hkChangeDisplaySettingsW(DEVMODEW* lpDevMode, DWORD dwFlags) { return hkChangeDisplaySettings_Impl(lpDevMode, dwFlags, oChangeDisplaySettingsW, "ChangeDisplaySettingsW"); }

        // Wrapper for Ex functions
        template <typename CharT, typename DevModeT, typename Func>
        LONG WINAPI hkChangeDisplaySettingsEx_Impl(const CharT* lpszDeviceName, DevModeT* lpDevMode, HWND hwnd, DWORD dwflags, LPVOID lParam, Func original)
        {
            if (WindowedMode::ShouldHandle() && !WindowedMode::g_State.inInternalChange) {
                if (lpDevMode) {
                     WindowedMode::NotifyResolutionChange(lpDevMode->dmPelsWidth, lpDevMode->dmPelsHeight);
                }
                return DISP_CHANGE_SUCCESSFUL;
            }
            return original(lpszDeviceName, lpDevMode, hwnd, dwflags, lParam);
        }

        LONG WINAPI hkChangeDisplaySettingsExA(LPCSTR lpszDeviceName, DEVMODEA* lpDevMode, HWND hwnd, DWORD dwflags, LPVOID lParam) { return hkChangeDisplaySettingsEx_Impl(lpszDeviceName, lpDevMode, hwnd, dwflags, lParam, oChangeDisplaySettingsExA); }
        LONG WINAPI hkChangeDisplaySettingsExW(LPCWSTR lpszDeviceName, DEVMODEW* lpDevMode, HWND hwnd, DWORD dwflags, LPVOID lParam) { return hkChangeDisplaySettingsEx_Impl(lpszDeviceName, lpDevMode, hwnd, dwflags, lParam, oChangeDisplaySettingsExW); }

        int WINAPI hkGetSystemMetrics(int nIndex)
        {
            if (nIndex == SM_CXSCREEN || nIndex == SM_CYSCREEN) {
                int dx = WindowedMode::g_State.targetDXVersion;
                if (dx == 9) {
                    Hooks::InstallD3D9CreateHooksLate();
                }
                else if (dx == 10 || dx == 11) {
                    Hooks::InstallDXGIHooksLate();
                }
            }

            if (WindowedMode::ShouldHandle()) {
                if (nIndex == SM_CXSCREEN) return WindowedMode::g_State.virtualWidth;
                if (nIndex == SM_CYSCREEN) return WindowedMode::g_State.virtualHeight;
            }
            return ((GetSystemMetrics_t)Data::oGetSystemMetrics)(nIndex);
        }

        BOOL WINAPI hkEnumDisplaySettingsA(LPCSTR lpszDeviceName, DWORD iModeNum, DEVMODEA* lpDevMode)
        {
            int dx = WindowedMode::g_State.targetDXVersion;
            if (dx == 9) {
                Hooks::InstallD3D9CreateHooksLate();
            }
            else if (dx == 10 || dx == 11) {
                Hooks::InstallDXGIHooksLate();
            }

            if (WindowedMode::ShouldHandle()) {
                if (iModeNum == 0 || iModeNum == ENUM_CURRENT_SETTINGS) {
                    memset(lpDevMode, 0, sizeof(DEVMODEA));
                    lpDevMode->dmSize = sizeof(DEVMODEA);
                    lpDevMode->dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL | DM_DISPLAYFREQUENCY;
                    lpDevMode->dmPelsWidth = WindowedMode::g_State.virtualWidth;
                    lpDevMode->dmPelsHeight = WindowedMode::g_State.virtualHeight;
                    lpDevMode->dmBitsPerPel = 32;
                    lpDevMode->dmDisplayFrequency = 60;
                    return TRUE;
                }
            }
            return oEnumDisplaySettingsA(lpszDeviceName, iModeNum, lpDevMode);
        }

        BOOL WINAPI hkEnumDisplaySettingsW(LPCWSTR lpszDeviceName, DWORD iModeNum, DEVMODEW* lpDevMode)
        {
            int dx = WindowedMode::g_State.targetDXVersion;
            if (dx == 9) {
                Hooks::InstallD3D9CreateHooksLate();
            }
            else if (dx == 10 || dx == 11) {
                Hooks::InstallDXGIHooksLate();
            }

            if (WindowedMode::ShouldHandle()) {
                if (iModeNum == 0 || iModeNum == ENUM_CURRENT_SETTINGS) {
                    memset(lpDevMode, 0, sizeof(DEVMODEW));
                    lpDevMode->dmSize = sizeof(DEVMODEW);
                    lpDevMode->dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL | DM_DISPLAYFREQUENCY;
                    lpDevMode->dmPelsWidth = WindowedMode::g_State.virtualWidth;
                    lpDevMode->dmPelsHeight = WindowedMode::g_State.virtualHeight;
                    lpDevMode->dmBitsPerPel = 32;
                    lpDevMode->dmDisplayFrequency = 60;
                    return TRUE;
                }
            }
            return oEnumDisplaySettingsW(lpszDeviceName, iModeNum, lpDevMode);
        }

        BOOL WINAPI hkAdjustWindowRectEx(LPRECT lpRect, DWORD dwStyle, BOOL bMenu, DWORD dwExStyle)
        {
            // If we are in windowed mode, we should probably use the virtual styles if the game is checking
            return ((AdjustWindowRectEx_t)Data::oAdjustWindowRectEx)(lpRect, dwStyle, bMenu, dwExStyle);
        }

        BOOL WINAPI hkGetCursorPos(LPPOINT lpPoint)
        {
            BOOL res = oGetCursorPos(lpPoint);
            if (res && WindowedMode::ShouldHandle() && Data::hWindow)
            {
                WindowedMode::ConvertPhysicalToVirtual((int&)lpPoint->x, (int&)lpPoint->y);
                return TRUE;
            }
            return res;
        }

        BOOL WINAPI hkSetCursorPos(int x, int y)
        {
            if (WindowedMode::ShouldHandle() && Data::hWindow && WindowedMode::g_State.virtualWidth > 0 && WindowedMode::g_State.virtualHeight > 0)
            {
                int px = x, py = y, pw = 0, ph = 0;
                WindowedMode::ConvertVirtualToPhysical(px, py, pw, ph, false);
                return oSetCursorPos(px, py);
            }

            return oSetCursorPos(x, y);
        }

        BOOL WINAPI hkClipCursor(const RECT* lpRect)
        {
            // 0=Default, 1=Confine, 2=Unlock, 3=UnlockWhenMenuOpen
            if (WindowedMode::g_State.cursorClipMode == 2) // Unlock
                return oClipCursor(NULL);

            if (WindowedMode::g_State.cursorClipMode == 3) // UnlockWhenMenuOpen
            {
                if (Data::bShowMenu)
                    return oClipCursor(NULL);
                // else fall through to Confine logic
            }

            if (WindowedMode::ShouldHandle() && WindowedMode::g_State.hWnd) {
                if (WindowedMode::g_State.cursorClipMode == 1 || WindowedMode::g_State.cursorClipMode == 3) // Confine or UnlockWhenMenuOpen (closed)
                {
                    // If game tries to unclip (lpRect=NULL), force clip to window.
                    if (!lpRect) {
                        RECT clientRect;
                        if (((GetClientRect_t)Data::oGetClientRect)(WindowedMode::g_State.hWnd, &clientRect)) {
                            POINT ul = { clientRect.left, clientRect.top };
                            POINT lr = { clientRect.right, clientRect.bottom };
                            ((ClientToScreen_t)Data::oClientToScreen)(WindowedMode::g_State.hWnd, &ul);
                            ((ClientToScreen_t)Data::oClientToScreen)(WindowedMode::g_State.hWnd, &lr);
                            RECT finalRect = { ul.x, ul.y, lr.x, lr.y };
                            return oClipCursor(&finalRect);
                        }
                    }
                }

                if (!lpRect) return oClipCursor(NULL);
                
                // Game clips to Virtual Screen. We clip to Physical Client.
                RECT clientRect;
                if (((GetClientRect_t)Data::oGetClientRect)(WindowedMode::g_State.hWnd, &clientRect)) {
                    POINT ul = { clientRect.left, clientRect.top };
                    POINT lr = { clientRect.right, clientRect.bottom };
                    ((ClientToScreen_t)Data::oClientToScreen)(WindowedMode::g_State.hWnd, &ul);
                    ((ClientToScreen_t)Data::oClientToScreen)(WindowedMode::g_State.hWnd, &lr);
                    RECT finalRect = { ul.x, ul.y, lr.x, lr.y };
                    return oClipCursor(&finalRect);
                }
            }
            return oClipCursor(lpRect);
        }

        BOOL WINAPI hkSetWindowPlacement(HWND hWnd, const WINDOWPLACEMENT *lpwndpl)
        {
            if (WindowedMode::ShouldHandle() && hWnd == Data::hWindow && !WindowedMode::g_State.inInternalChange) {
                if (WindowedMode::g_State.resizeBehavior == WindowedMode::ResizeBehavior::ScaleContent) {
                    if (lpwndpl) {
                        WINDOWPLACEMENT modified = *lpwndpl;
                        modified.flags &= ~WPF_RESTORETOMAXIMIZED;
                        
                        // Enforce windowed position
                        if (WindowedMode::g_State.windowX != -1 && WindowedMode::g_State.windowY != -1) {
                            modified.rcNormalPosition.left = WindowedMode::g_State.windowX;
                            modified.rcNormalPosition.top = WindowedMode::g_State.windowY;
                            modified.rcNormalPosition.right = modified.rcNormalPosition.left + WindowedMode::g_State.windowWidth;
                            modified.rcNormalPosition.bottom = modified.rcNormalPosition.top + WindowedMode::g_State.windowHeight;
                        }
                        return oSetWindowPlacement(hWnd, &modified);
                    }
                }
            }
            return oSetWindowPlacement(hWnd, lpwndpl);
        }
        
        // --- Get/Set Window Long (Block style changes) ---
        template <typename ResultT, typename Func>
        ResultT WINAPI hkGetWindowLong_Impl(HWND hWnd, int nIndex, Func original) {
            ResultT res = original(hWnd, nIndex);
            if (WindowedMode::ShouldHandle() && hWnd == Data::hWindow) {
                if (nIndex == GWL_STYLE) {
                    if (WindowedMode::g_State.activeMode == WindowedMode::Mode::BorderlessFullscreen || 
                        WindowedMode::g_State.activeMode == WindowedMode::Mode::Borderless)
                    {
                        res = (res & ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU)) | WS_POPUP;
                    }
                    else if (WindowedMode::g_State.activeMode == WindowedMode::Mode::Bordered)
                    {
                        res = (res & ~WS_POPUP) | WS_OVERLAPPEDWINDOW;
                    }
                }
            }
            return res;
        }

        LONG WINAPI hkGetWindowLongA(HWND hWnd, int nIndex) { return hkGetWindowLong_Impl<LONG>(hWnd, nIndex, oGetWindowLongA); }
        LONG_PTR WINAPI hkGetWindowLongPtrA(HWND hWnd, int nIndex) { return hkGetWindowLong_Impl<LONG_PTR>(hWnd, nIndex, oGetWindowLongPtrA); }
        LONG WINAPI hkGetWindowLongW(HWND hWnd, int nIndex) { return hkGetWindowLong_Impl<LONG>(hWnd, nIndex, oGetWindowLongW); }
        LONG_PTR WINAPI hkGetWindowLongPtrW(HWND hWnd, int nIndex) { return hkGetWindowLong_Impl<LONG_PTR>(hWnd, nIndex, oGetWindowLongPtrW); }

        template <typename ResultT, typename Func, typename GetFunc>
        ResultT WINAPI hkSetWindowLong_Impl(HWND hWnd, int nIndex, ResultT dwNewLong, Func original, GetFunc getter) {
            if (WindowedMode::ShouldHandle() && hWnd == Data::hWindow && !WindowedMode::g_State.inInternalChange && nIndex == GWL_STYLE) {
                 return getter(hWnd, GWL_STYLE);
            }
            return original(hWnd, nIndex, dwNewLong);
        }
        
        LONG WINAPI hkSetWindowLongA(HWND hWnd, int nIndex, LONG dwNewLong) { return hkSetWindowLong_Impl(hWnd, nIndex, dwNewLong, oSetWindowLongA, oGetWindowLongA); }
        LONG_PTR WINAPI hkSetWindowLongPtrA(HWND hWnd, int nIndex, LONG_PTR dwNewLong) { return hkSetWindowLong_Impl(hWnd, nIndex, dwNewLong, oSetWindowLongPtrA, oGetWindowLongPtrA); }
        LONG WINAPI hkSetWindowLongW(HWND hWnd, int nIndex, LONG dwNewLong) { return hkSetWindowLong_Impl(hWnd, nIndex, dwNewLong, oSetWindowLongW, oGetWindowLongW); }
        LONG_PTR WINAPI hkSetWindowLongPtrW(HWND hWnd, int nIndex, LONG_PTR dwNewLong) { return hkSetWindowLong_Impl(hWnd, nIndex, dwNewLong, oSetWindowLongPtrW, oGetWindowLongPtrW); }

    } // namespace Hooks

    namespace WindowedMode
    {
        static bool s_User32Hooked = false;

        void InstallHooks()
        {            
            if (!s_User32Hooked) {
                LOG_INFO("Installing User32 Hooks...");
                HMODULE hUser = GetModuleHandleA("user32.dll");
                using namespace Hooks;
                if (hUser) {
                    Data::oGetSystemMetrics = GetProcAddress(hUser, "GetSystemMetrics");
                    Data::oAdjustWindowRectEx = GetProcAddress(hUser, "AdjustWindowRectEx");
                    Data::oGetClientRect = GetProcAddress(hUser, "GetClientRect");
                    Data::oGetWindowRect = GetProcAddress(hUser, "GetWindowRect");
                    Data::oClientToScreen = GetProcAddress(hUser, "ClientToScreen");
                    Data::oScreenToClient = GetProcAddress(hUser, "ScreenToClient");

                    oCreateWindowExA = (CreateWindowExA_t)GetProcAddress(hUser, "CreateWindowExA");
                    oCreateWindowExW = (CreateWindowExW_t)GetProcAddress(hUser, "CreateWindowExW");
                    oSetWindowPos = (SetWindowPos_t)GetProcAddress(hUser, "SetWindowPos");
                    oMoveWindow = (MoveWindow_t)GetProcAddress(hUser, "MoveWindow");
                    oChangeDisplaySettingsA = (ChangeDisplaySettingsA_t)GetProcAddress(hUser, "ChangeDisplaySettingsA");
                    oChangeDisplaySettingsW = (ChangeDisplaySettingsW_t)GetProcAddress(hUser, "ChangeDisplaySettingsW");
                    oChangeDisplaySettingsExA = (ChangeDisplaySettingsExA_t)GetProcAddress(hUser, "ChangeDisplaySettingsExA");
                    oChangeDisplaySettingsExW = (ChangeDisplaySettingsExW_t)GetProcAddress(hUser, "ChangeDisplaySettingsExW");
                    oEnumDisplaySettingsA = (EnumDisplaySettingsA_t)GetProcAddress(hUser, "EnumDisplaySettingsA");
                    oEnumDisplaySettingsW = (EnumDisplaySettingsW_t)GetProcAddress(hUser, "EnumDisplaySettingsW");
                    oGetCursorPos = (GetCursorPos_t)GetProcAddress(hUser, "GetCursorPos");
                    oSetCursorPos = (SetCursorPos_t)GetProcAddress(hUser, "SetCursorPos");
                    oClipCursor = (ClipCursor_t)GetProcAddress(hUser, "ClipCursor");
                    oSetWindowPlacement = (SetWindowPlacement_t)GetProcAddress(hUser, "SetWindowPlacement");
                    oGetWindowLongA = (GetWindowLongA_t)GetProcAddress(hUser, "GetWindowLongA");
                    oGetWindowLongW = (GetWindowLongW_t)GetProcAddress(hUser, "GetWindowLongW");
                    oSetWindowLongA = (SetWindowLongA_t)GetProcAddress(hUser, "SetWindowLongA");
                    oSetWindowLongW = (SetWindowLongW_t)GetProcAddress(hUser, "SetWindowLongW");

                    oGetWindowLongPtrA = (GetWindowLongPtrA_t)GetProcAddress(hUser, "GetWindowLongPtrA");
                    oGetWindowLongPtrW = (GetWindowLongPtrW_t)GetProcAddress(hUser, "GetWindowLongPtrW");
                    oSetWindowLongPtrA = (SetWindowLongPtrA_t)GetProcAddress(hUser, "SetWindowLongPtrA");
                    oSetWindowLongPtrW = (SetWindowLongPtrW_t)GetProcAddress(hUser, "SetWindowLongPtrW");

                    // Helper to hook safe imports
                    auto Hook = [&](void* addr, void* hook, void** orig) {
                        if (addr && hook) {
                            if (MH_CreateHook(addr, hook, orig) == MH_OK) {
                                MH_EnableHook(addr);
                            }
                        }
                    };

                    Hook((void*)oCreateWindowExA, hkCreateWindowExA, (void**)&oCreateWindowExA);
                    Hook((void*)oCreateWindowExW, hkCreateWindowExW, (void**)&oCreateWindowExW);
                    Hook((void*)oSetWindowPos, hkSetWindowPos, (void**)&oSetWindowPos);
                    Hook((void*)oMoveWindow, hkMoveWindow, (void**)&oMoveWindow);
                    Hook((void*)oChangeDisplaySettingsA, hkChangeDisplaySettingsA, (void**)&oChangeDisplaySettingsA);
                    Hook((void*)oChangeDisplaySettingsW, hkChangeDisplaySettingsW, (void**)&oChangeDisplaySettingsW);
                    Hook((void*)oChangeDisplaySettingsExA, hkChangeDisplaySettingsExA, (void**)&oChangeDisplaySettingsExA);
                    Hook((void*)oChangeDisplaySettingsExW, hkChangeDisplaySettingsExW, (void**)&oChangeDisplaySettingsExW);
                    Hook((void*)Data::oGetSystemMetrics, hkGetSystemMetrics, (void**)&Data::oGetSystemMetrics);
                    Hook((void*)oEnumDisplaySettingsA, hkEnumDisplaySettingsA, (void**)&oEnumDisplaySettingsA);
                    Hook((void*)oEnumDisplaySettingsW, hkEnumDisplaySettingsW, (void**)&oEnumDisplaySettingsW);
                    Hook((void*)Data::oAdjustWindowRectEx, hkAdjustWindowRectEx, (void**)&Data::oAdjustWindowRectEx);
                    Hook((void*)oGetCursorPos, hkGetCursorPos, (void**)&oGetCursorPos);
                    Data::oGetCursorPos = (FARPROC)oGetCursorPos;
                    Hook((void*)oSetCursorPos, hkSetCursorPos, (void**)&oSetCursorPos);
                    Hook((void*)oClipCursor, hkClipCursor, (void**)&oClipCursor);
                    Hook((void*)oSetWindowPlacement, hkSetWindowPlacement, (void**)&oSetWindowPlacement);
                    Hook((void*)Data::oScreenToClient, hkScreenToClient, (void**)&Data::oScreenToClient);
                    Hook((void*)Data::oClientToScreen, hkClientToScreen, (void**)&Data::oClientToScreen);
                    Hook((void*)Data::oGetClientRect, hkGetClientRect, (void**)&Data::oGetClientRect);
                    Hook((void*)Data::oGetWindowRect, hkGetWindowRect, (void**)&Data::oGetWindowRect);
                    Hook((void*)oGetWindowLongA, hkGetWindowLongA, (void**)&oGetWindowLongA);
                    Hook((void*)oGetWindowLongW, hkGetWindowLongW, (void**)&oGetWindowLongW);
                    Hook((void*)oGetWindowLongPtrA, hkGetWindowLongPtrA, (void**)&oGetWindowLongPtrA);
                    Hook((void*)oGetWindowLongPtrW, hkGetWindowLongPtrW, (void**)&oGetWindowLongPtrW);
                    Hook((void*)oSetWindowLongA, hkSetWindowLongA, (void**)&oSetWindowLongA);
                    Hook((void*)oSetWindowLongW, hkSetWindowLongW, (void**)&oSetWindowLongW);
                    Hook((void*)oSetWindowLongPtrA, hkSetWindowLongPtrA, (void**)&oSetWindowLongPtrA);
                    Hook((void*)oSetWindowLongPtrW, hkSetWindowLongPtrW, (void**)&oSetWindowLongPtrW);

                    s_User32Hooked = true;
                }
            }

            bool wantD3D9 = (WindowedMode::g_State.targetDXVersion == 0 || WindowedMode::g_State.targetDXVersion == 9);
            bool wantDXGI = (WindowedMode::g_State.targetDXVersion == 0 || WindowedMode::g_State.targetDXVersion == 10 || WindowedMode::g_State.targetDXVersion == 11);
            bool wantD3D10 = (WindowedMode::g_State.targetDXVersion == 0 || WindowedMode::g_State.targetDXVersion == 10);
            bool wantD3D11 = (WindowedMode::g_State.targetDXVersion == 0 || WindowedMode::g_State.targetDXVersion == 11);

            WindowedMode::InstallD3D9HooksIfNeeded(wantD3D9);
            WindowedMode::InstallDXGIHooksIfNeeded(wantDXGI);
            WindowedMode::InstallD3D10HooksIfNeeded(wantD3D10);
            WindowedMode::InstallD3D11HooksIfNeeded(wantD3D11);
        }
    } // namespace WindowedMode
} // namespace BaseHook
