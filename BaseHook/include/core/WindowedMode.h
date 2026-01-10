#pragma once
#include <Windows.h>
#include <atomic>
#include <vector>
#include <string>

namespace BaseHook
{
    namespace WindowedMode
    {
        enum class Mode : int
        {
            ExclusiveFullscreen = 0,
            BorderlessFullscreen = 1,
            Borderless = 2,
            Bordered = 3
        };

        enum class ResizeBehavior : int
        {
            ResizeWindow = 0,   // Window Size & Virtual Resolution matched to Game Resolution
            ScaleContent = 1    // Window Size fixed to config, Virtual Resolution matched to Game Resolution
        };

        enum class CursorClipMode : int
        {
            Default = 0,
            Confine = 1,
            Unlock = 2,
            UnlockWhenMenuOpen = 3
        };

        enum class ViewportScalingMode : int
        {
            None = 0,          // Maintain physical screen position/size (virtual pos/size updates)
            ScalePhysical = 1  // Maintain relative virtual position/size (physical window size updates)
        };

        struct MonitorInfo {
            HMONITOR handle;
            RECT rect;
            RECT workRect;
            std::string name;
            bool isPrimary;
        };

        struct State
        {
            Mode activeMode = Mode::BorderlessFullscreen;
            Mode queuedMode = Mode::BorderlessFullscreen;
            ResizeBehavior resizeBehavior = ResizeBehavior::ResizeWindow;
            int virtualWidth = 1920;
            int virtualHeight = 1080;
            int windowX = -1;
            int windowY = -1;
            int windowWidth = 1920;
            int windowHeight = 1080;
            int targetMonitor = 0;
            int cursorClipMode = 0; // 0=Default, 1=Confine, 2=Unlock, 3=UnlockWhenMenuOpen
            int overrideWidth = 0;
            int overrideHeight = 0;

            int targetDXVersion = 0; // 0=Auto

            bool alwaysOnTop = false;

            HWND hWnd = NULL;
            bool isInitialized = false;
            
            // To prevent recursion in hooks
            std::atomic<bool> inInternalChange = false;
            std::atomic<bool> needMonitorRefresh = false;

            // --- Runtime detection / enforcement (DXGI) ---
            // 0 = unknown/unset, 1 = exclusive fullscreen, 2 = not-exclusive (windowed/borderless)
            std::atomic<int> detectedFullscreenState = 0;
            std::atomic<bool> requestRestoreExclusiveFullscreen = false;

            // Drag State
            bool isDragging = false;
            POINT dragStartCursor = { 0 };
            RECT dragStartWindow = { 0 };

            // Multi-viewport state
            bool enableMultiViewport = false;
            ViewportScalingMode viewportScaling = ViewportScalingMode::ScalePhysical;

            // True when user is dragging via OS title bar (WM_ENTERSIZEMOVE to WM_EXITSIZEMOVE)
            bool isSystemMoving = false;
        };
        extern State g_State;

        // RAII guard to safely set/reset inInternalChange flag
        // Prevents bugs from early returns forgetting to reset the flag
        struct InternalChangeGuard {
            InternalChangeGuard() { g_State.inInternalChange = true; }
            ~InternalChangeGuard() { g_State.inInternalChange = false; }
            InternalChangeGuard(const InternalChangeGuard&) = delete;
            InternalChangeGuard& operator=(const InternalChangeGuard&) = delete;
        };

        // Initialization
        void EarlyInit(Mode mode, ResizeBehavior resize, int x, int y, int w, int h, int dx, int monitorIdx, int clipMode, int overrideW, int overrideH, bool alwaysOnTop);
        void Init();
        void InstallHooks(); // Called by InstallEarlyHooks
        
        // Late / On-Demand Hooks
        void InstallD3D9HooksIfNeeded(bool wantD3D9);
        void InstallD3D9HooksLate();
        void InstallDXGIHooksLate();
        void InstallDXGIHooksIfNeeded(bool wantDXGI);
        void InstallD3D10HooksIfNeeded(bool wantD3D10);
        void InstallD3D11HooksIfNeeded(bool wantD3D11);

        // Runtime Control
        void Apply(HWND hWnd);
        bool ShouldHandle();
        bool ShouldApplyResolutionOverride();
        void SetManagedWindow(HWND hWnd);

        // Runtime Settings Update
        void SetSettings(Mode mode, ResizeBehavior resizeBehavior, int x, int y, int w, int h, int monitorIdx, int clipMode, int overrideW, int overrideH, bool alwaysOnTop);

        void TriggerFakeReset();

        // Check if pending changes require application during Reset/Create
        void CheckAndApplyPendingState();

        // --- Runtime state detection / enforcement (DXGI) ---
        // Returns true if the DXGI swapchain is currently in exclusive fullscreen.
        // If DXGI isn't active/available yet, returns false.
        bool IsActuallyExclusiveFullscreen();

        // Returns true if we have a detected state from DXGI (swapchain exists and query succeeded).
        bool HasDetectedFullscreenState();

        // A coarse detected state to drive UI: true=exclusive fullscreen, false=not exclusive.
        // If HasDetectedFullscreenState() is false, value is undefined.
        bool IsDetectedExclusiveFullscreen();

        // Called from WndProc when focus is regained, to restore exclusive fullscreen if configured.
        void RequestRestoreExclusiveFullscreen();

        // Called from the render thread (Present path). Polls DXGI fullscreen state and performs
        // restore if requested (and configured exclusive fullscreen).
        void TickDXGIState();

        // Called from the render thread (EndScene path) for DX9.
        void TickDX9State();

        // Updates the detected state for DX9 based on Reset/Create calls
        void UpdateDetectedStateDX9(bool exclusive);

        // Resolution Management
        void GetVirtualResolution(int& width, int& height);
        void GetWindowResolution(int& width, int& height);
        void NotifyResolutionChange(int width, int height);
        void GetFixedDesktopResolution(int& w, int& h);

        // Monitor Management
        const std::vector<MonitorInfo>& GetMonitors();
        int GetCurrentMonitorIndex();
        int GetPrimaryMonitorIndex();

        // Coordinate transforms (Virtual <-> Physical Client)
        // NOTE: Uses original (unhooked) User32 calls via BaseHook::Data to avoid recursion.
        // Replaces local helpers in WindowHooks.
        bool PhysicalClientToVirtual(HWND hWnd, POINT& pt);
        bool VirtualClientToPhysical(HWND hWnd, POINT& pt);
        void ConvertVirtualToPhysical(int& x, int& y, int& w, int& h, bool scaleSize);
        void ConvertPhysicalToVirtual(int& x, int& y);

        // Optimized logic (exposed for advanced use cases if needed, primarily used efficiently in cpp)
        void ConvertVirtualToPhysical(int& x, int& y, int& w, int& h, bool scaleSize, const RECT& mainClientRect, const POINT& mainClientToScreenOffset);
        void ConvertPhysicalToVirtual(int& x, int& y, const RECT& mainClientRect, const POINT& mainClientToScreenOffset);

        // Helper to scale mouse message LPARAM (client coords) from physical to virtual
        LPARAM ScaleMouseMessage(HWND hWnd, UINT uMsg, LPARAM lParam);

        // Window Management
        void HandleDrag(bool isDragging);
        void SyncStateFromWindow(); // Called after title bar drag to sync position

        // Multi-viewport support
        bool IsMainViewportWindow(HWND hWnd);
        bool IsImGuiPlatformWindow(HWND hWnd);
        bool IsMultiViewportEnabled();
        void SetMultiViewportEnabled(bool enabled);
        void SetMultiViewportScalingMode(ViewportScalingMode mode);
        void RefreshPlatformWindows();
    }
}
