#include "PluginLoaderApp.h"

#include "core/BaseHook.h"
#include "CpuAffinity.h"
#include "PluginLoaderConfig.h"
#include "ImGuiCTX.h"
#include "ImGuiConfigUtils.h"
#include "KeyBind.h"
#include "core/WindowedMode.h"
#include "crash_handler.h"
#include "log.h"
#include "InputCapture.h"
#include "util/FramerateLimiter.h"

#include <windows.h>

#include "imgui.h"
#include "imgui_impl_win32.h"

#include "MainMenu.h"

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

PluginLoaderApp* PluginLoaderApp::s_instance = nullptr;

namespace
{
    bool IsMouseButtonsOrWheelMessage(UINT msg)
    {
        switch (msg)
        {
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
            return true;
        default:
            return false;
        }
    }

    bool ShouldImGuiMouseButtonsUseDirectInput()
    {
        return PluginLoaderConfig::g_Config.ImGuiMouseSource.get() == PluginLoaderConfig::ImGuiMouseInputSource::DirectInput;
    }

    // Sink that only pushes to the on-screen console.
    void LogConsoleSink(const char* text)
    {
        if (auto* app = PluginLoaderApp::Get())
            app->GetConsole().AddLogF("%s", text);
    }

    // Plugins call this for "console" logging; our Log system fans out to file + sinks.
    void LogUnified(const char* text)
    {
        Log::Write("%s", text);
    }

    ImGuiContext* GetImGuiContext_Impl()
    {
        return ImGui::GetCurrentContext();
    }

    Game GetCurrentGame_Impl()
    {
        auto* app = PluginLoaderApp::Get();
        return app ? app->GetPluginManager().GetCurrentGame() : Game::Unknown;
    }

    void* GetPluginInterface_Impl(const char* pluginName)
    {
        auto* app = PluginLoaderApp::Get();
        return app ? app->GetPluginManager().GetPluginInterface(pluginName) : nullptr;
    }

    void PluginLoaderInterface_RequestUnload(HMODULE /*pluginHandle*/)
    {
        LOG_WARN("RequestUnload is not implemented. Plugins cannot be unloaded at runtime.");
    }

    // Helper for polling hotkeys with "rising edge" detection
    struct HotkeyPoller
    {
        KeyBind lastBind;
        bool wasDown = false;

        // Returns true if the action should trigger (pressed this frame, wasn't pressed last frame)
        bool Update(const KeyBind& currentBind)
        {
            // Reset state if bind changes to prevent accidental triggers
            if (currentBind != lastBind)
            {
                if (currentBind.IsPressed()) wasDown = true;
                lastBind = currentBind;
            }

            bool pressed = currentBind.IsPressed();
            bool triggered = pressed && !wasDown;
            wasDown = pressed;
            return triggered;
        }
    };

    LRESULT __stdcall LoaderWndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        bool renderInBackground = PluginLoaderConfig::g_Config.RenderInBackground.get();
        const bool configuredExclusiveFullscreen =
            (PluginLoaderConfig::g_Config.WindowedMode.get() == PluginLoaderConfig::WindowedMode::ExclusiveFullscreen);

        if (configuredExclusiveFullscreen)
            renderInBackground = false;

        HWND hWndOther = (uMsg == WM_ACTIVATE) ? (HWND)lParam : NULL;
        bool toImGui = (uMsg == WM_ACTIVATE && BaseHook::WindowedMode::IsImGuiPlatformWindow(hWndOther));

        // 1. Exclusive Fullscreen Handling: minimize on focus loss (unless focusing an ImGui window), restore on focus regain.
        if (configuredExclusiveFullscreen && !renderInBackground && !toImGui)
        {
            if (uMsg == WM_ACTIVATE)
            {
                if (LOWORD(wParam) == WA_INACTIVE) { if (IsWindow(hWnd) && !IsIconic(hWnd)) ShowWindow(hWnd, SW_MINIMIZE); }
                else { if (IsWindow(hWnd) && IsIconic(hWnd)) ShowWindow(hWnd, SW_RESTORE); }
            }
            else if (uMsg == WM_ACTIVATEAPP)
            {
                if (wParam == FALSE) { if (IsWindow(hWnd) && !IsIconic(hWnd)) ShowWindow(hWnd, SW_MINIMIZE); }
                else { if (IsWindow(hWnd) && IsIconic(hWnd)) ShowWindow(hWnd, SW_RESTORE); }
            }
        }

        // 2. Authoritative Settings Enforcement
        if ((uMsg == WM_SYSKEYDOWN || uMsg == WM_SYSKEYUP || uMsg == WM_SYSCHAR) &&
            wParam == VK_RETURN && (lParam & (1 << 29)) && configuredExclusiveFullscreen)
            return 1;

        // 3. Update Global Input Blocking State
        {
            const HWND foreground = GetForegroundWindow();
            const bool is_focused = (foreground == BaseHook::Data::hWindow);
            const ConsoleMode cm = (PluginLoaderApp::Get() ? PluginLoaderApp::Get()->GetConsole().mode : ConsoleMode::Hidden);
            const bool consoleWantsFocus = (cm == ConsoleMode::ForegroundAndFocusable);
            
            BaseHook::Data::bShowConsole = consoleWantsFocus;

            const auto cap = InputCapture::Compute(is_focused, BaseHook::Data::bShowMenu, cm);
            if (BaseHook::Data::bBlockInput != (cap.blockKeyboardWndProc || cap.blockMouseMoveWndProc || cap.blockDirectInputMouseButtons))
                BaseHook::Data::bBlockInput = (cap.blockKeyboardWndProc || cap.blockMouseMoveWndProc || cap.blockDirectInputMouseButtons);
        }

        // 4. Delegate everything else to the library (ImGui, Scaling, Spoofing, Input Blocking, etc)
        return BaseHook::Hooks::WndProc_Base(hWnd, uMsg, wParam, lParam);
    }
}

struct PluginLoaderApp::LoaderSettings : public BaseHook::Settings
{
    explicit LoaderSettings(WndProc_t wndProc)
        : BaseHook::Settings(wndProc, true)
    {
        m_WindowedMode = (int)PluginLoaderConfig::g_Config.WindowedMode.get();
        m_ResizeBehavior = (int)PluginLoaderConfig::g_Config.ResizeBehavior.get();
        m_WindowPosX = PluginLoaderConfig::g_Config.WindowPosX.get();
        m_WindowPosY = PluginLoaderConfig::g_Config.WindowPosY.get();
        m_WindowWidth = PluginLoaderConfig::g_Config.WindowWidth.get();
        m_WindowHeight = PluginLoaderConfig::g_Config.WindowHeight.get();
        m_DirectXVersion = (int)PluginLoaderConfig::g_Config.DirectXVersion.get();
        m_CursorClipMode = (int)PluginLoaderConfig::g_Config.CursorClipMode.get();
        m_RenderInBackground = PluginLoaderConfig::g_Config.RenderInBackground.get();
        
        // Sync multi-viewport setting
        BaseHook::WindowedMode::SetMultiViewportEnabled(PluginLoaderConfig::g_Config.EnableMultiViewport.get());
        BaseHook::WindowedMode::SetMultiViewportScalingMode(PluginLoaderConfig::g_Config.MultiViewportScaling.get());
    }

    void OnActivate() override {}
    void OnDetach() override {}

    void DrawOverlay() override
    {
        ImGuiIO& io = ImGui::GetIO();
        io.FontGlobalScale = (float)PluginLoaderConfig::g_Config.fontSize.get() / 13.0f;

        if (auto* app = PluginLoaderApp::Get())
        {
            if (app->GetLoaderInterface().m_ImGuiContext == nullptr)
                app->GetLoaderInterface().m_ImGuiContext = ImGui::GetCurrentContext();
        }

        const HWND foreground = GetForegroundWindow();
        const bool is_focused = (foreground == BaseHook::Data::hWindow);

        // --- Controller/Keyboard Hotkey Polling ---
        // Only poll if window is focused and ImGui doesn't want keyboard input
        if (is_focused && !io.WantCaptureKeyboard)
        {
             static HotkeyPoller s_menuPoller;
             static HotkeyPoller s_consolePoller;

             if (s_menuPoller.Update(PluginLoaderConfig::g_Config.hotkey_ToggleMenu.get()))
             {
                 BaseHook::Data::bShowMenu = !BaseHook::Data::bShowMenu;
             }

             if (s_consolePoller.Update(PluginLoaderConfig::g_Config.hotkey_ToggleConsole.get()))
             {
                 if (auto* app = PluginLoaderApp::Get())
                     app->GetConsole().ToggleVisibility();
             }
        }
        // --------------------------------

        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

        if (auto* app = PluginLoaderApp::Get())
            app->GetConsole().Draw("Console");

        const ConsoleMode cm = (PluginLoaderApp::Get() ? PluginLoaderApp::Get()->GetConsole().mode : ConsoleMode::Hidden);
        BaseHook::Data::bShowConsole = (cm == ConsoleMode::ForegroundAndFocusable);

        BaseHook::Data::bImGuiMouseButtonsFromDirectInput = ShouldImGuiMouseButtonsUseDirectInput();

        const auto cap = InputCapture::Compute(is_focused, BaseHook::Data::bShowMenu, cm);

        // Cursor clip mode: 0=Default, 1=Confine, 2=Unlock, 3=UnlockWhenMenuOpen
        const int cursorClipMode = BaseHook::WindowedMode::g_State.cursorClipMode;

        const bool shouldBlock = cap.blockKeyboardWndProc || cap.blockMouseMoveWndProc || cap.blockDirectInputMouseButtons;
        if (BaseHook::Data::bBlockInput != shouldBlock)
        {
            BaseHook::Data::bBlockInput = shouldBlock;
            LOG_INFO("Input blocking state changed to: %s", BaseHook::Data::bBlockInput ? "ON" : "OFF");
        }

        // Confine cursor logic: Only clip when cursorClipMode requires it
        bool shouldClipCursor = false;
        
        if (cursorClipMode == 1) // Confine - always clip when focused
        {
            shouldClipCursor = true;
        }
        else if (cursorClipMode == 3) // UnlockWhenMenuOpen - clip only if menu/console not open
        {
            shouldClipCursor = !(BaseHook::Data::bShowMenu || BaseHook::Data::bShowConsole);
        }
        // cursorClipMode 0 (Default) and 2 (Unlock): don't proactively clip from DrawOverlay

        if (shouldClipCursor && is_focused && BaseHook::Data::hWindow)
        {
            RECT rect;
            if (GetClientRect(BaseHook::Data::hWindow, &rect))
            {
                POINT ul = { rect.left, rect.top };
                POINT lr = { rect.right, rect.bottom };
                ClientToScreen(BaseHook::Data::hWindow, &ul);
                ClientToScreen(BaseHook::Data::hWindow, &lr);
                rect = { ul.x, ul.y, lr.x, lr.y };
                ClipCursor(&rect);
            }
        }
        else if (!shouldClipCursor)
        {
            // Explicitly unclip when unclip mode is active
            ClipCursor(NULL);
        }

        if (auto* app = PluginLoaderApp::Get())
            app->GetPluginManager().UpdatePlugins();
    }

    void DrawMenu() override
    {
        auto* app = PluginLoaderApp::Get();
        if (!app)
            return;
        static Ui::MainMenu menu;
        menu.Draw(app->GetPluginManager());
    }
};

PluginLoaderApp::PluginLoaderApp(HMODULE module)
    : m_module(module)
{
    s_instance = this;
}

PluginLoaderApp::~PluginLoaderApp()
{
    if (s_instance == this)
        s_instance = nullptr;
}

PluginLoaderApp* PluginLoaderApp::Get()
{
    return s_instance;
}

void PluginLoaderApp::Init()
{
    Log::AddSink(LogConsoleSink);
    CrashHandler::Init();

    LOG_INFO("Plugin Loader Attached.");

    m_loaderInterface.GetCurrentGame = GetCurrentGame_Impl;
    m_loaderInterface.LogToConsole = LogUnified;
    m_loaderInterface.RequestUnloadPlugin = PluginLoaderInterface_RequestUnload;
    m_loaderInterface.GetImGuiContext = GetImGuiContext_Impl;
    m_loaderInterface.GetPluginInterface = GetPluginInterface_Impl;

    // Apply CPU affinity if a custom mask is set. 
    // If 0, we do nothing here and let plugins (like EaglePatch) handle defaults.
    if (PluginLoaderConfig::g_Config.CpuAffinityMask != 0)
    {
        CpuAffinity::Apply(PluginLoaderConfig::g_Config.CpuAffinityMask);
    }

    // Init default framerate settings
    BaseHook::g_FramerateLimiter.SetEnabled(PluginLoaderConfig::g_Config.EnableFPSLimit);
    BaseHook::g_FramerateLimiter.SetTargetFPS(static_cast<double>(PluginLoaderConfig::g_Config.FPSLimit));

    // Initialize BaseHook (and hooks) BEFORE loading plugins.
    // This prevents race conditions where a plugin initializes controllers/input
    // before our blocking hooks are installed.
    BaseHook::Data::thisDLLModule = m_module;
    m_settings = std::make_unique<LoaderSettings>(LoaderWndProc);
    
    // Connect KeyBind system to BaseHook's Virtual Input
    KeyBind::SetInputProvider(BaseHook::Hooks::TryGetVirtualXInputState);

    BaseHook::Start(m_settings.get());
    LOG_INFO("Basehook initialized successfully.");

    // Now load plugins
    m_pluginManager.Init(m_module, m_loaderInterface);
}

void PluginLoaderApp::Tick()
{
    PluginLoaderConfig::CheckHotReload();
}

void PluginLoaderApp::RequestShutdown()
{
    m_shutdownRequested = true;
}

bool PluginLoaderApp::IsShutdownRequested() const
{
    return m_shutdownRequested;
}

void PluginLoaderApp::Shutdown()
{
    LOG_INFO("Shutdown initiated.");
    m_pluginManager.ShutdownPlugins();
    BaseHook::Detach();
    Log::RemoveSink(LogConsoleSink);
    CrashHandler::Shutdown();
    Log::Shutdown();
}


