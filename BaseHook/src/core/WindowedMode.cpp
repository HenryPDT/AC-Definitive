#include "pch.h"
#include "core/WindowedMode.h"
#include "core/BaseHook.h"
#include "hooks/Hooks.h"
#include "hooks/WindowHooks.h"
#include "log.h"
#include "util/ComPtr.h"
#include "util/RenderDetection.h"
#include <cstdio>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <cmath>

namespace BaseHook
{
    namespace WindowedMode
    {
        State g_State;
        std::vector<MonitorInfo> g_Monitors;

        // Helper to get friendly name using DisplayConfig API
        static std::string GetFriendlyMonitorName(const char* deviceName)
        {
            UINT32 pathCount = 0, modeCount = 0;
            if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &pathCount, &modeCount) != ERROR_SUCCESS)
                return "Generic Monitor";

            std::vector<DISPLAYCONFIG_PATH_INFO> paths(pathCount);
            std::vector<DISPLAYCONFIG_MODE_INFO> modes(modeCount);

            if (QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &pathCount, paths.data(), &modeCount, modes.data(), nullptr) != ERROR_SUCCESS)
                return "Generic Monitor";

            for (const auto& path : paths)
            {
                DISPLAYCONFIG_SOURCE_DEVICE_NAME sourceName{};
                sourceName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
                sourceName.header.size = sizeof(DISPLAYCONFIG_SOURCE_DEVICE_NAME);
                sourceName.header.adapterId = path.sourceInfo.adapterId;
                sourceName.header.id = path.sourceInfo.id;

                if (DisplayConfigGetDeviceInfo(&sourceName.header) == ERROR_SUCCESS)
                {
                    char sourceGdiName[32];
                    WideCharToMultiByte(CP_ACP, 0, sourceName.viewGdiDeviceName, -1, sourceGdiName, 32, nullptr, nullptr);

                    if (strcmp(deviceName, sourceGdiName) == 0)
                    {
                        DISPLAYCONFIG_TARGET_DEVICE_NAME targetName{};
                        targetName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
                        targetName.header.size = sizeof(DISPLAYCONFIG_TARGET_DEVICE_NAME);
                        targetName.header.adapterId = path.targetInfo.adapterId;
                        targetName.header.id = path.targetInfo.id;

                        if (DisplayConfigGetDeviceInfo(&targetName.header) == ERROR_SUCCESS)
                        {
                            if (targetName.flags.friendlyNameFromEdid)
                            {
                                int size = WideCharToMultiByte(CP_UTF8, 0, targetName.monitorFriendlyDeviceName, -1, nullptr, 0, nullptr, nullptr);
                                std::string name(size, '\0');
                                WideCharToMultiByte(CP_UTF8, 0, targetName.monitorFriendlyDeviceName, -1, &name[0], size, nullptr, nullptr);
                                if (!name.empty() && name.back() == '\0') name.pop_back();
                                return name;
                            }
                        }
                    }
                }
            }
            return "Generic Monitor";
        }

        static BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC, LPRECT, LPARAM)
        {
            MonitorInfo info{};
            info.handle = hMonitor;
            
            MONITORINFOEXA mi{};
            mi.cbSize = sizeof(mi);
            if (GetMonitorInfoA(hMonitor, &mi))
            {
                info.rect = mi.rcMonitor;
                info.workRect = mi.rcWork;
                info.isPrimary = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;
                
                // Get the real hardware name instead of GDI device string
                info.name = GetFriendlyMonitorName(mi.szDevice);
                
                // Append the display number for clarity if the name is still generic
                if (info.name == "Generic Monitor") {
                    info.name = std::string("Display ") + mi.szDevice;
                }
            }
            g_Monitors.push_back(info);
            return TRUE;
        }

        static void RefreshMonitors()
        {
            g_Monitors.clear();
            EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, 0);
            
            // Sort by X coordinate left-to-right
            std::sort(g_Monitors.begin(), g_Monitors.end(), [](const MonitorInfo& a, const MonitorInfo& b) {
                return a.rect.left < b.rect.left;
            });
        }

        const std::vector<MonitorInfo>& GetMonitors()
        {
            if (g_Monitors.empty() || g_State.needMonitorRefresh.exchange(false)) RefreshMonitors();
            return g_Monitors;
        }

        int GetCurrentMonitorIndex()
        {
            const auto& monitors = GetMonitors();

            HMONITOR hMon = nullptr;
            if (g_State.hWnd && IsWindow(g_State.hWnd))
                hMon = MonitorFromWindow(g_State.hWnd, MONITOR_DEFAULTTONEAREST);
            else if (Data::hWindow && IsWindow(Data::hWindow))
                hMon = MonitorFromWindow(Data::hWindow, MONITOR_DEFAULTTONEAREST);
            else
                hMon = MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);

            for (int i = 0; i < (int)monitors.size(); ++i)
            {
                if (monitors[i].handle == hMon)
                    return i;
            }
            return 0;
        }

        int GetPrimaryMonitorIndex()
        {
            const auto& monitors = GetMonitors();
            for (int i = 0; i < (int)monitors.size(); ++i)
            {
                if (monitors[i].isPrimary)
                    return i;
            }
            return 0;
        }

        static bool GetDesktopRectForWindow(HWND hWnd, int& outX, int& outY, int& outW, int& outH)
        {
            // Use target monitor logic if available
            const auto& mons = GetMonitors();
            if (g_State.targetMonitor >= 0 && g_State.targetMonitor < (int)mons.size())
            {
                const auto& m = mons[g_State.targetMonitor];
                outX = m.rect.left;
                outY = m.rect.top;
                outW = m.rect.right - m.rect.left;
                outH = m.rect.bottom - m.rect.top;
                return true;
            }

            HMONITOR mon = nullptr;
            if (hWnd && IsWindow(hWnd))
                mon = MonitorFromWindow(hWnd, MONITOR_DEFAULTTOPRIMARY);
            else
                mon = MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);

            MONITORINFOEXA mi{};
            mi.cbSize = sizeof(mi);
            if (!mon || !GetMonitorInfoA(mon, &mi))
                return false;

            const int monW = (int)(mi.rcMonitor.right - mi.rcMonitor.left);
            const int monH = (int)(mi.rcMonitor.bottom - mi.rcMonitor.top);

            int w = monW;
            int h = monH;

            DEVMODEA dm{};
            dm.dmSize = sizeof(dm);
            if (EnumDisplaySettingsExA(mi.szDevice, ENUM_REGISTRY_SETTINGS, &dm, 0))
            {
                // Use registry settings to avoid "stale" exclusive mode dimensions.
                w = (int)dm.dmPelsWidth;
                h = (int)dm.dmPelsHeight;
            }

            outX = (int)mi.rcMonitor.left;
            outY = (int)mi.rcMonitor.top;
            outW = w > 0 ? w : monW;
            outH = h > 0 ? h : monH;
            return true;
        }

        static bool QueryExclusiveFullscreen(bool& outExclusive)
        {
            outExclusive = false;
            if (ShouldHandle())
                return true;

            if (Data::pSwapChain)
            {
                BOOL fs = FALSE;
                HRESULT hr = Data::pSwapChain->GetFullscreenState(&fs, nullptr);
                if (SUCCEEDED(hr))
                {
                    outExclusive = (fs != FALSE);
                    return true;
                }
            }
            else if (Data::pDevice)
            {
                // In DX9, we check the primary swapchain's present parameters.
                ComPtr<IDirect3DSwapChain9> pSwap;
                if (SUCCEEDED(Data::pDevice->GetSwapChain(0, pSwap.GetAddressOf())))
                {
                    D3DPRESENT_PARAMETERS pp;
                    if (SUCCEEDED(pSwap->GetPresentParameters(&pp)))
                    {
                        outExclusive = !pp.Windowed;
                        return true;
                    }
                }
            }

            return false;
        }

        bool IsActuallyExclusiveFullscreen()
        {
            bool exclusive = false;
            if (!QueryExclusiveFullscreen(exclusive))
                return false;
            return exclusive;
        }

        bool HasDetectedFullscreenState()
        {
            return g_State.detectedFullscreenState.load() != 0;
        }

        bool IsDetectedExclusiveFullscreen()
        {
            return g_State.detectedFullscreenState.load() == 1;
        }

        void RequestRestoreExclusiveFullscreen()
        {
            g_State.requestRestoreExclusiveFullscreen.store(true);
        }

        void TickDXGIState()
        {
            // Only meaningful for DXGI (DX10/DX11).
            bool exclusive = false;
            const bool haveState = QueryExclusiveFullscreen(exclusive);
            if (haveState)
            {
                g_State.detectedFullscreenState.store(exclusive ? 1 : 2);
            }
            else
            {
                // Keep last known state if any; don't spam with unknown.
            }

            // If we're configured for exclusive fullscreen, ensure we don't drift into windowed
            // while focused. Alt+Tab will temporarily force windowed; we restore on focus regain.
            if (ShouldHandle())
                return; // user selected a windowed mode; never force exclusive.

            if (!Data::hWindow || !IsWindow(Data::hWindow))
                return;

            const HWND foreground = GetForegroundWindow();
            const bool is_focused = (foreground == Data::hWindow);
            if (!is_focused)
                return;

            // If we don't have state yet, nothing we can safely do.
            if (!haveState)
                return;

            if (exclusive)
            {
                // Clear pending restore requests once we're back in exclusive.
                g_State.requestRestoreExclusiveFullscreen.store(false);
                return;
            }

            // Not exclusive but configured exclusive + focused.
            // Restore if requested (WM_ACTIVATE) or if the game silently toggled.
            const bool requested = g_State.requestRestoreExclusiveFullscreen.exchange(false);

            static ULONGLONG s_lastAttemptMs = 0;
            const ULONGLONG now = GetTickCount64();
            const ULONGLONG minIntervalMs = requested ? 0 : 750; // be more aggressive on focus regain
            if ((now - s_lastAttemptMs) < minIntervalMs)
                return;
            s_lastAttemptMs = now;

            if (g_State.inInternalChange)
                return;

            LOG_INFO("WindowedMode: Restoring Exclusive Fullscreen (DXGI). requested=%d", requested ? 1 : 0);
            Data::pSwapChain->SetFullscreenState(TRUE, nullptr);
        }

        void TickDX9State()
        {
            // DX9 equivalent of TickDXGIState
            bool exclusive = false;
            const bool haveState = QueryExclusiveFullscreen(exclusive);
            if (haveState)
            {
                g_State.detectedFullscreenState.store(exclusive ? 1 : 2);
            }

            if (ShouldHandle())
                return;

            if (!Data::hWindow || !IsWindow(Data::hWindow))
                return;

            const HWND foreground = GetForegroundWindow();
            const bool is_focused = (foreground == Data::hWindow);
            if (!is_focused)
                return;

            if (!haveState)
                return;

            if (exclusive)
            {
                g_State.requestRestoreExclusiveFullscreen.store(false);
                return;
            }

            // In DX9, we can't easily force fullscreen without a Reset.
            // But if we're focused and NOT in fullscreen, and we should be,
            // we can trigger a fake reset to get back there.
            const bool requested = g_State.requestRestoreExclusiveFullscreen.exchange(false);

            static ULONGLONG s_lastAttemptMs = 0;
            const ULONGLONG now = GetTickCount64();
            const ULONGLONG minIntervalMs = requested ? 0 : 2000; // DX9 reset is heavier, be conservative
            if ((now - s_lastAttemptMs) < minIntervalMs)
                return;
            s_lastAttemptMs = now;

            if (g_State.inInternalChange || Data::g_fakeResetState != BaseHook::FakeResetState::Clear)
                return;

            LOG_INFO("WindowedMode: Restoring Exclusive Fullscreen (DX9) via Fake Reset. requested=%d", requested ? 1 : 0);
            TriggerFakeReset();
        }

        void UpdateDetectedStateDX9(bool exclusive)
        {
            if (ShouldHandle())
                exclusive = false;
            g_State.detectedFullscreenState.store(exclusive ? 1 : 2);
        }

        void EarlyInit(Mode mode, ResizeBehavior resize, int x, int y, int w, int h, int dx, int monitorIdx, int clipMode, int overrideW, int overrideH, bool alwaysOnTop)
        {
            if (dx == 0) {
                auto type = Util::GetGameSpecificRenderType();
                if (type == kiero::RenderType::D3D9) dx = 9;
                else if (type == kiero::RenderType::D3D10) dx = 10;
                else if (type == kiero::RenderType::D3D11) dx = 11;

                if (dx != 0) {
                    LOG_INFO("EarlyInit: Auto-Detected DirectX Version: %d", dx);
                }
            }

            g_State.activeMode = mode;
            g_State.queuedMode = mode;
            g_State.resizeBehavior = resize;
            
            g_State.windowWidth = w;
            g_State.windowHeight = h;
            g_State.windowX = x;
            g_State.windowY = y;

            g_State.overrideWidth = overrideW;
            g_State.overrideHeight = overrideH;
            g_State.cursorClipMode = clipMode;
            g_State.virtualWidth = w;
            g_State.virtualHeight = h;
            g_State.targetDXVersion = dx;
            g_State.targetMonitor = monitorIdx;
            g_State.alwaysOnTop = alwaysOnTop;

            LOG_INFO("EarlyInit: Applied Settings. Mode=%d, DX=%d, Size=%dx%d, Monitor=%d", 
                (int)g_State.activeMode, g_State.targetDXVersion, g_State.windowWidth, g_State.windowHeight, g_State.targetMonitor);
        }

        bool ShouldHandle()
        {
            return g_State.activeMode != Mode::ExclusiveFullscreen;
        }

        bool ShouldApplyResolutionOverride()
        {
            return g_State.overrideWidth > 0 && g_State.overrideHeight > 0;
        }

        void SetManagedWindow(HWND hWnd)
        {
            if (g_State.hWnd != hWnd) {
                g_State.hWnd = hWnd;
            }
        }

        void Apply(HWND hWnd)
        {
            if (!ShouldHandle() || g_State.inInternalChange || !hWnd || !IsWindow(hWnd)) return;

            // SAFETY: If the game changed display settings (resolution) before we hooked it,
            // we need to revert to desktop settings to ensure our windowed mode renders correctly.
            // We do this cheaply by checking if we haven't done it recently.
            static bool s_DisplaySettingsReset = false;
            if (!s_DisplaySettingsReset) {
                LOG_INFO("WindowedMode::Apply: Resetting display to desktop default.");
                ChangeDisplaySettingsA(NULL, 0);
                s_DisplaySettingsReset = true;
            }

            g_State.hWnd = hWnd;
            
            // Optimization: Read current state first (Bypass hooks to get TRUE style)
            DWORD currentStyle = Hooks::GetTrueWindowStyle(hWnd);
            DWORD currentExStyle = Hooks::GetTrueWindowExStyle(hWnd);

            RECT currentRect;
            GetWindowRect(hWnd, &currentRect);

            RECT realRect = currentRect;
            if (Data::oGetWindowRect)
                ((BOOL(WINAPI*)(HWND, LPRECT))Data::oGetWindowRect)(hWnd, &realRect);

            DWORD targetStyle = currentStyle;
            DWORD targetExStyle = currentExStyle;

            // Strip fullscreen/popup styles if not in bordered mode
            if (g_State.activeMode == Mode::BorderlessFullscreen || g_State.activeMode == Mode::Borderless)
            {
                targetStyle &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
                targetStyle |= WS_POPUP;
            }
            else if (g_State.activeMode == Mode::Bordered)
            {
                targetStyle |= WS_OVERLAPPEDWINDOW;
                targetStyle &= ~WS_POPUP;
            }

            if (g_State.alwaysOnTop) {
                targetExStyle |= WS_EX_TOPMOST;
            }
            else {
                targetExStyle &= ~WS_EX_TOPMOST;
            }

            int x = g_State.windowX;
            int y = g_State.windowY;
            int w = g_State.windowWidth;
            int h = g_State.windowHeight;

            int screenW = Data::oGetSystemMetrics ? ((GetSystemMetrics_t)Data::oGetSystemMetrics)(SM_CXSCREEN) : GetSystemMetrics(SM_CXSCREEN);
            int screenH = Data::oGetSystemMetrics ? ((GetSystemMetrics_t)Data::oGetSystemMetrics)(SM_CYSCREEN) : GetSystemMetrics(SM_CYSCREEN);

            // Resolve Target Monitor Rect if applicable
            RECT monitorRect = { 0, 0, screenW, screenH };
            const auto& mons = GetMonitors();
            if (g_State.targetMonitor >= 0 && g_State.targetMonitor < (int)mons.size())
            {
                monitorRect = mons[g_State.targetMonitor].rect;
                // If x/y are default, we map them relative to this monitor
                if (screenW != (monitorRect.right - monitorRect.left)) {
                     screenW = monitorRect.right - monitorRect.left;
                     screenH = monitorRect.bottom - monitorRect.top;
                }
            }

            if (g_State.activeMode == Mode::BorderlessFullscreen)
            {
                int monX = 0, monY = 0, monW = 0, monH = 0;
                if (GetDesktopRectForWindow(hWnd, monX, monY, monW, monH))
                {
                    x = monX;
                    y = monY;
                    w = monW;
                    h = monH;
                }
                else
                {
                    GetFixedDesktopResolution(w, h);
                    x = 0; y = 0;
                }
            }
            else
            {
                // Calculate window rect based on client size
                RECT rect = { 0, 0, w, h };
                if (Data::oAdjustWindowRectEx) ((AdjustWindowRectEx_t)Data::oAdjustWindowRectEx)(&rect, targetStyle, FALSE, targetExStyle);
                else AdjustWindowRectEx(&rect, targetStyle, FALSE, targetExStyle);
                
                int fullW = rect.right - rect.left;
                int fullH = rect.bottom - rect.top;

                // Center relative to the target monitor
                if (x == -1) x = monitorRect.left + (screenW - fullW) / 2;
                if (y == -1) y = monitorRect.top + (screenH - fullH) / 2;

                w = fullW;
                h = fullH;
            }

            // Check if changes are needed
            bool styleChanged = (currentStyle != targetStyle) || (currentExStyle != targetExStyle);
            // In windowed handling, user32 GetWindowRect may be hooked to return "virtual" size.
            // For resize decisions, compare against the real physical window rect via the original API if available.
            const RECT& compareRect = (Data::oGetWindowRect ? realRect : currentRect);
            bool posChanged = (compareRect.left != x) || (compareRect.top != y) ||
                              ((compareRect.right - compareRect.left) != w) ||
                              ((compareRect.bottom - compareRect.top) != h);

            if (!styleChanged && !posChanged)
                return;
            
            LOG_INFO("WindowedMode::Apply: Applying changes (Style=%d, Pos=%d) to HWND %p", styleChanged, posChanged, hWnd);

            g_State.inInternalChange = true;

            if (styleChanged) {
                SetWindowLong(hWnd, GWL_STYLE, targetStyle);
                SetWindowLong(hWnd, GWL_EXSTYLE, targetExStyle);
            }

            // Always call SetWindowPos if style changed to force frame redraw, or if pos changed
            HWND insertAfter = g_State.alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST;
            SetWindowPos(hWnd, insertAfter, x, y, w, h, SWP_FRAMECHANGED | SWP_SHOWWINDOW);

            g_State.inInternalChange = false;
            g_State.isInitialized = true;
            LOG_INFO("WindowedMode::Apply: Applied to HWND %p", hWnd);
        }
        
        void SetSettings(Mode mode, ResizeBehavior resizeBehavior, int x, int y, int w, int h, int monitorIdx, int clipMode, int overrideW, int overrideH, bool alwaysOnTop)
        {
            // Enforce logic: Borderless Fullscreen always behaves like "Scale Content" (Window fixed to screen, content scales)
            // This prevents confusion if the user previously had "ResizeWindow" active.
            if (mode == Mode::BorderlessFullscreen)
            {
                resizeBehavior = ResizeBehavior::ScaleContent;
            }

            g_State.queuedMode = mode;
            g_State.resizeBehavior = resizeBehavior;
            g_State.windowX = x;
            g_State.windowY = y;
            g_State.targetMonitor = monitorIdx;
            g_State.cursorClipMode = clipMode;
            g_State.overrideWidth = overrideW;
            g_State.overrideHeight = overrideH;
            g_State.alwaysOnTop = alwaysOnTop;

            // If switching between windowed modes, apply immediately
            if (g_State.activeMode != Mode::ExclusiveFullscreen && g_State.queuedMode != Mode::ExclusiveFullscreen)
            {
                g_State.activeMode = g_State.queuedMode;
            }

            // If we are resizing window to match game, try to ensure virtual resolution is up-to-date
            // by querying the graphics device if available. This fixes the glitch where the window
            // momentarily takes the config size instead of the game size when switching from Exclusive Fullscreen.
            if (g_State.resizeBehavior == ResizeBehavior::ResizeWindow)
            {
                int devW = 0, devH = 0;
                if (Data::pSwapChain)
                {
                    DXGI_SWAP_CHAIN_DESC desc;
                    if (SUCCEEDED(Data::pSwapChain->GetDesc(&desc)))
                    {
                        devW = desc.BufferDesc.Width;
                        devH = desc.BufferDesc.Height;
                    }
                }
                else if (Data::pDevice)
                {
                    D3DVIEWPORT9 vp;
                    if (SUCCEEDED(Data::pDevice->GetViewport(&vp)))
                    {
                        devW = vp.Width;
                        devH = vp.Height;
                    }
                }

                if (devW > 0 && devH > 0)
                {
                    if (g_State.virtualWidth != devW || g_State.virtualHeight != devH)
                    {
                        // Update virtual resolution to match actual device resolution
                        NotifyResolutionChange(devW, devH);
                    }
                }
            }

            // If we are resizing window to match game, enforce that now using current virtual res
            // But only if NOT Borderless Fullscreen (redundant check due to override above, but safe)
            if (g_State.resizeBehavior == ResizeBehavior::ResizeWindow &&
                g_State.activeMode != Mode::BorderlessFullscreen)
            {
                // Only if virtual resolution is known/valid
                if (g_State.virtualWidth > 0 && g_State.virtualHeight > 0) {
                    g_State.windowWidth = g_State.virtualWidth;
                    g_State.windowHeight = g_State.virtualHeight;
                }
            } else {
                g_State.windowWidth = w;
                g_State.windowHeight = h;
            }

            // Only apply if we are currently handling the window
            if (ShouldHandle())
            {
                Apply(g_State.hWnd);
            }
        }

        void ForceUpdateDXGI()
        {
            if (Data::pSwapChain)
            {
                CheckAndApplyPendingState();

                // If currently handling (Windowed/Borderless), enforce Windowed state on SwapChain
                if (ShouldHandle())
                {
                    LOG_INFO("ForceUpdateDXGI: Setting swap chain to windowed.");
                    Data::pSwapChain->SetFullscreenState(FALSE, NULL);
                    
                    // If we have an override resolution, explicitly request a ResizeTarget.
                    // This ensures the game receives a window message/event indicating the "desired" size has changed,
                    // prompting it to call ResizeBuffers (hitting our hooks).
                    if (g_State.overrideWidth > 0 && g_State.overrideHeight > 0)
                    {
                        DXGI_MODE_DESC target = {};
                        target.Width = g_State.overrideWidth;
                        target.Height = g_State.overrideHeight;
                        // Leave Format/RefreshRate zero to keep current/unknown

                        LOG_INFO("ForceUpdateDXGI: Requesting ResizeTarget to %dx%d", target.Width, target.Height);
                        Data::pSwapChain->ResizeTarget(&target);
                    }

                    // Force re-application of window styles
                    Apply(g_State.hWnd);
                }
                else
                {
                    // If switching to Exclusive Fullscreen
                     LOG_INFO("ForceUpdateDXGI: Setting swap chain to exclusive fullscreen.");
                     Data::pSwapChain->SetFullscreenState(TRUE, NULL);

                     // Request immediate restoration in TickDXGIState
                     RequestRestoreExclusiveFullscreen();

                     // If we have an override resolution, we must enforce it in Exclusive Fullscreen as well.
                     // Just setting FullscreenState=TRUE isn't enough if we are already fullscreen.
                     if (ShouldApplyResolutionOverride())
                     {
                         DXGI_MODE_DESC target = {};
                         target.Width = g_State.overrideWidth;
                         target.Height = g_State.overrideHeight;
                         target.RefreshRate.Numerator = 0;
                         target.RefreshRate.Denominator = 0;
                         target.Format = DXGI_FORMAT_UNKNOWN;

                         LOG_INFO("ForceUpdateDXGI: Requesting ResizeTarget (Exclusive) to %dx%d", target.Width, target.Height);
                         Data::pSwapChain->ResizeTarget(&target);
                     }
                }
            }
            else
            {
                LOG_WARN("ForceUpdateDXGI: pSwapChain is NULL.");
            }
        }

        void TriggerFakeReset()
        {
            if (g_State.targetDXVersion == 9 || (g_State.targetDXVersion == 0 && !Data::pSwapChain)) {
                LOG_INFO("Triggering D3D9 fake device reset.");
                Data::g_fakeResetState = BaseHook::FakeResetState::Initiate;
                RequestRestoreExclusiveFullscreen();
            }
            else {
                LOG_INFO("Triggering DXGI state update.");
                ForceUpdateDXGI();
            }
        }

        void CheckAndApplyPendingState()
        {
            if (g_State.queuedMode != g_State.activeMode)
            {
                LOG_INFO("WindowedMode: Applying pending mode change (%d -> %d)", (int)g_State.activeMode, (int)g_State.queuedMode);

                bool wasHandling = ShouldHandle();
                g_State.activeMode = g_State.queuedMode;
                bool isHandling = ShouldHandle();

                // Transitioning FROM Exclusive Fullscreen TO Windowed
                if (!wasHandling && isHandling)
                {
                    LOG_INFO("WindowedMode: Transitioning to windowed. Restoring desktop resolution.");
                    g_State.inInternalChange = true;
                    ChangeDisplaySettings(NULL, 0);
                    g_State.inInternalChange = false;
                }

                // Transitioning FROM Windowed TO Exclusive Fullscreen
                // We must strictly restore window styles so D3D Reset works correctly.
                if (wasHandling && !isHandling && g_State.hWnd && IsWindow(g_State.hWnd))
                {
                    LOG_INFO("WindowedMode: Reverting to exclusive fullscreen defaults.");
                    g_State.inInternalChange = true;

                    DWORD dwStyle = GetWindowLong(g_State.hWnd, GWL_STYLE);
                    DWORD dwExStyle = GetWindowLong(g_State.hWnd, GWL_EXSTYLE);

                    // Restore standard styles, remove popup/topmost
                    SetWindowLong(g_State.hWnd, GWL_STYLE, (dwStyle & ~WS_POPUP) | WS_OVERLAPPEDWINDOW);
                    SetWindowLong(g_State.hWnd, GWL_EXSTYLE, dwExStyle & ~WS_EX_TOPMOST);

                    // Apply without moving/sizing (Game will handle sizing next)
                    SetWindowPos(g_State.hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, 
                        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);

                    g_State.inInternalChange = false;
                }
            }
        }

        void GetVirtualResolution(int& width, int& height)
        {
            width = g_State.virtualWidth;
            height = g_State.virtualHeight;
        }

        static bool GetPhysicalClientRect(HWND hWnd, RECT& outRect)
        {
            if (!hWnd || !IsWindow(hWnd))
                return false;

            if (Data::oGetClientRect)
                return ((GetClientRect_t)Data::oGetClientRect)(hWnd, &outRect) != FALSE;

            return ::GetClientRect(hWnd, &outRect) != FALSE;
        }

        void GetWindowResolution(int& width, int& height)
        {
            if (g_State.hWnd && IsWindow(g_State.hWnd))
            {
                RECT rc;
                if (GetPhysicalClientRect(g_State.hWnd, rc))
                {
                    width = rc.right - rc.left;
                    height = rc.bottom - rc.top;
                    return;
                }
            }

            width = g_State.windowWidth;
            height = g_State.windowHeight;
        }

        void GetFixedDesktopResolution(int& w, int& h)
        {
            // Fallback: raw primary screen metrics (unhooked), if monitor query fails.
            const int screenW = Data::oGetSystemMetrics ? ((GetSystemMetrics_t)Data::oGetSystemMetrics)(SM_CXSCREEN) : GetSystemMetrics(SM_CXSCREEN);
            const int screenH = Data::oGetSystemMetrics ? ((GetSystemMetrics_t)Data::oGetSystemMetrics)(SM_CYSCREEN) : GetSystemMetrics(SM_CYSCREEN);

            // Use registry settings to get the desktop mode even if the game briefly switched to exclusive fullscreen.
            // Prefer the monitor the game window is on (multi-monitor correctness).
            HMONITOR mon = nullptr;
            if (g_State.hWnd && IsWindow(g_State.hWnd))
                mon = MonitorFromWindow(g_State.hWnd, MONITOR_DEFAULTTOPRIMARY);
            else
                mon = MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);

            MONITORINFOEXA mi{};
            mi.cbSize = sizeof(mi);
            if (mon && GetMonitorInfoA(mon, &mi))
            {
                DEVMODEA dm{};
                dm.dmSize = sizeof(dm);
                if (EnumDisplaySettingsExA(mi.szDevice, ENUM_REGISTRY_SETTINGS, &dm, 0))
                {
                    w = (int)dm.dmPelsWidth;
                    h = (int)dm.dmPelsHeight;
                    return;
                }
            }

            // Last resort: primary desktop registry settings (works fine for single-monitor setups).
            {
                DEVMODEA dm{};
                dm.dmSize = sizeof(dm);
                if (EnumDisplaySettingsA(NULL, ENUM_REGISTRY_SETTINGS, &dm))
                {
                    w = (int)dm.dmPelsWidth;
                    h = (int)dm.dmPelsHeight;
                    return;
                }
            }

            w = screenW;
            h = screenH;
        }

        void NotifyResolutionChange(int width, int height)
        {
            if (width <= 0 || height <= 0) return;

            // In Borderless Fullscreen, the window size is locked to the screen size.
            // But we MUST update virtualWidth/Height so the game knows the correct backbuffer size
            // for coordinate translation and viewport setup.

            bool changed = false;
            // Only resize the window if configured to do so AND not in Borderless Fullscreen (which locks to desktop)
            // Also force update if in Exclusive Fullscreen, as window size must match resolution.
            bool shouldResizeWindow = (g_State.resizeBehavior == ResizeBehavior::ResizeWindow);
            if (g_State.activeMode == Mode::ExclusiveFullscreen) shouldResizeWindow = true;
            if (g_State.activeMode == Mode::BorderlessFullscreen) shouldResizeWindow = false;

            if (shouldResizeWindow)
            {
                if (g_State.windowWidth != width || g_State.windowHeight != height) {
                    g_State.windowWidth = width;
                    g_State.windowHeight = height;
                    changed = true;
                }
            }

            g_State.virtualWidth = width;
            g_State.virtualHeight = height;

            if (changed && g_State.hWnd) {
                LOG_INFO("Resolution Change Detected: Resizing Window to %dx%d", width, height);
                Apply(g_State.hWnd);
            } else {
                LOG_INFO("Resolution Change Detected: Virtual=%dx%d, Window=%dx%d", width, height, g_State.windowWidth, g_State.windowHeight);
            }
        }

        void HandleDrag(bool isDragging)
        {
            if (isDragging && !g_State.isDragging)
            {
                if (g_State.hWnd)
                {
                    if (Data::oGetCursorPos) ((BOOL(WINAPI*)(LPPOINT))Data::oGetCursorPos)(&g_State.dragStartCursor);
                    else GetCursorPos(&g_State.dragStartCursor);

                    if (Data::oGetWindowRect) ((BOOL(WINAPI*)(HWND, LPRECT))Data::oGetWindowRect)(g_State.hWnd, &g_State.dragStartWindow);
                    else GetWindowRect(g_State.hWnd, &g_State.dragStartWindow);
                }
            }

            g_State.isDragging = isDragging;

            if (isDragging)
            {
                if (g_State.hWnd)
                {
                    POINT curr;
                    if (Data::oGetCursorPos) ((BOOL(WINAPI*)(LPPOINT))Data::oGetCursorPos)(&curr);
                    else GetCursorPos(&curr);

                    int dx = curr.x - g_State.dragStartCursor.x;
                    int dy = curr.y - g_State.dragStartCursor.y;

                    int newX = g_State.dragStartWindow.left + dx;
                    int newY = g_State.dragStartWindow.top + dy;
                    
                    int newMonIdx = g_State.targetMonitor;
                    RECT newRect = { newX, newY, newX + (g_State.dragStartWindow.right - g_State.dragStartWindow.left), newY + (g_State.dragStartWindow.bottom - g_State.dragStartWindow.top) };
                    HMONITOR hMon = MonitorFromRect(&newRect, MONITOR_DEFAULTTONEAREST);

                    const auto& mons = GetMonitors();
                    for(int i = 0; i < (int)mons.size(); ++i) {
                        if (mons[i].handle == hMon) {
                            newMonIdx = i;
                            break;
                        }
                    }

                    // Pass current bg/top settings
                    SetSettings(g_State.activeMode, g_State.resizeBehavior, newX, newY, g_State.windowWidth, g_State.windowHeight, newMonIdx,
                        g_State.cursorClipMode, g_State.overrideWidth, g_State.overrideHeight, g_State.alwaysOnTop);
                }
            }
        }

        bool PhysicalClientToVirtual(HWND hWnd, POINT& pt)
        {
            if ((!ShouldHandle() && !ShouldApplyResolutionOverride()) || hWnd != g_State.hWnd)
                return false;

            RECT clientRect{};
            if (!GetPhysicalClientRect(hWnd, clientRect))
                return false;

            const int clientW = clientRect.right - clientRect.left;
            const int clientH = clientRect.bottom - clientRect.top;
            
            if (clientW <= 0 || clientH <= 0 || g_State.virtualWidth <= 0 || g_State.virtualHeight <= 0)
                return false;

            pt.x = (LONG)std::lroundf((float)pt.x * ((float)g_State.virtualWidth / (float)clientW));
            pt.y = (LONG)std::lroundf((float)pt.y * ((float)g_State.virtualHeight / (float)clientH));
            return true;
        }

        // Unified helper previously in WindowHooks.cpp
        void ConvertVirtualToPhysical(int& x, int& y, int& w, int& h, bool scaleSize)
        {
            if (!Data::hWindow) return;

            POINT pt = { x, y };
            // Virtual Client (Lie Screen) -> Physical Client
            VirtualClientToPhysical(Data::hWindow, pt);

            // Physical Client -> Physical Screen
            // We need to use the unhooked function to get true physical screen coords
            if (Data::oClientToScreen)
                ((BOOL(WINAPI*)(HWND, LPPOINT))Data::oClientToScreen)(Data::hWindow, &pt);
            else
                ::ClientToScreen(Data::hWindow, &pt);

            x = pt.x;
            y = pt.y;

            if (scaleSize) {
                int vW, vH;
                GetVirtualResolution(vW, vH);
                RECT rc;
                GetPhysicalClientRect(Data::hWindow, rc);
                int pW = rc.right - rc.left;
                int pH = rc.bottom - rc.top;
                if (vW > 0 && pW > 0) {
                    w = (int)std::lroundf((float)w * (float)pW / (float)vW);
                    h = (int)std::lroundf((float)h * (float)pH / (float)vH);
                }
            }
        }

        void ConvertPhysicalToVirtual(int& x, int& y)
        {
            if (!Data::hWindow) return;
            POINT pt = { x, y };
            // Physical Screen -> Physical Client
            if (Data::oScreenToClient)
                ((BOOL(WINAPI*)(HWND, LPPOINT))Data::oScreenToClient)(Data::hWindow, &pt);
            else
                ::ScreenToClient(Data::hWindow, &pt);

            // Physical Client -> Virtual Client (Lie Screen)
            PhysicalClientToVirtual(Data::hWindow, pt);
            x = pt.x;
            y = pt.y;
        }

        bool VirtualClientToPhysical(HWND hWnd, POINT& pt)
        {
            if ((!ShouldHandle() && !ShouldApplyResolutionOverride()) || hWnd != g_State.hWnd)
                return false;

            RECT clientRect{};
            if (!GetPhysicalClientRect(hWnd, clientRect))
                return false;

            const int clientW = clientRect.right - clientRect.left;
            const int clientH = clientRect.bottom - clientRect.top;

            if (clientW <= 0 || clientH <= 0 || g_State.virtualWidth <= 0 || g_State.virtualHeight <= 0)
                return false;

            pt.x = (LONG)std::lroundf((float)pt.x * ((float)clientW / (float)g_State.virtualWidth));
            pt.y = (LONG)std::lroundf((float)pt.y * ((float)clientH / (float)g_State.virtualHeight));
            return true;
        }

        LPARAM ScaleMouseMessage(HWND hWnd, UINT uMsg, LPARAM lParam)
        {
            if ((!ShouldHandle() && !ShouldApplyResolutionOverride())) return lParam;
            if (hWnd != g_State.hWnd) return lParam;

            // Check for client-coordinate mouse messages
            bool isMouse = (uMsg == WM_MOUSEMOVE ||
                (uMsg >= WM_LBUTTONDOWN && uMsg <= WM_MBUTTONDBLCLK) ||
                (uMsg >= WM_XBUTTONDOWN && uMsg <= WM_XBUTTONDBLCLK));

            if (!isMouse) return lParam;

            // Extract signed 16-bit values manually to avoid windowsx.h dependency
            POINT pt = { (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam) };

            if (PhysicalClientToVirtual(hWnd, pt)) {
                return MAKELPARAM((short)pt.x, (short)pt.y);
            }
            return lParam;
        }
    }
}