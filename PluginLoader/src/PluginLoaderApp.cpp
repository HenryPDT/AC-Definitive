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
        // Not implemented for now
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

        // Force RenderInBackground off if in Exclusive Fullscreen to ensure standard minimizing behavior
        if (configuredExclusiveFullscreen)
            renderInBackground = false;

        // When running Exclusive Fullscreen, Alt+Tab will force DXGI out of exclusive mode.
        // We can't prevent that, but we can behave like a "traditional" fullscreen game:
        // minimize on focus loss so the window doesn't sit on the desktop rendering in the background,
        // and restore/re-enter exclusive on focus regain.
        if (configuredExclusiveFullscreen && !renderInBackground)
        {
            if (uMsg == WM_ACTIVATE)
            {
                if (LOWORD(wParam) == WA_INACTIVE)
                {
                    if (IsWindow(hWnd) && !IsIconic(hWnd))
                        ShowWindow(hWnd, SW_MINIMIZE);
                }
                else
                {
                    if (IsWindow(hWnd) && IsIconic(hWnd))
                        ShowWindow(hWnd, SW_RESTORE);
                }
            }
            else if (uMsg == WM_ACTIVATEAPP)
            {
                // Some titles primarily use WM_ACTIVATEAPP.
                if (wParam == FALSE)
                {
                    if (IsWindow(hWnd) && !IsIconic(hWnd))
                        ShowWindow(hWnd, SW_MINIMIZE);
                }
                else
                {
                    if (IsWindow(hWnd) && IsIconic(hWnd))
                        ShowWindow(hWnd, SW_RESTORE);
                }
            }
        }

        // Block Alt+Enter toggling when Exclusive Fullscreen is configured. This keeps settings authoritative
        // and prevents silent drift into windowed mode.
        if ((uMsg == WM_SYSKEYDOWN || uMsg == WM_SYSKEYUP || uMsg == WM_SYSCHAR) &&
            wParam == VK_RETURN &&
            (lParam & (1 << 29)) && // ALT is down
            configuredExclusiveFullscreen)
        {
            return 1;
        }

        // Background Rendering: Spoof focus messages to keep game loop running
        if (renderInBackground)
        {
            if (uMsg == WM_ACTIVATE && LOWORD(wParam) == WA_INACTIVE)
                wParam = WA_ACTIVE; // Pretend we are active
            if (uMsg == WM_ACTIVATEAPP && wParam == FALSE)
                wParam = TRUE; // Pretend app is active
        }

        // On focus regain, request exclusive fullscreen restore if configured.
        if (uMsg == WM_ACTIVATE &&
            LOWORD(wParam) != WA_INACTIVE &&
            configuredExclusiveFullscreen)
        {
            BaseHook::WindowedMode::RequestRestoreExclusiveFullscreen();
        }

        // Scale mouse coordinates from Physical Window to Virtual Resolution (if applicable)
        // Fixes cursor restriction when window size != game resolution (Scale Content mode).
        lParam = BaseHook::WindowedMode::ScaleMouseMessage(hWnd, uMsg, lParam);

        // Feed ImGui. To avoid double mouse input, optionally filter mouse messages when DirectInput drives ImGui mouse.
        if (BaseHook::Data::bIsInitialized)
        {
            // Always allow Win32 mouse movement into ImGui; only filter buttons/wheel when DirectInput injects them.
            if (!ShouldImGuiMouseButtonsUseDirectInput() || !IsMouseButtonsOrWheelMessage(uMsg))
                ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
        }

        // Handle our own hotkeys, but only if an ImGui window doesn't want keyboard input.
        if (BaseHook::Data::bIsInitialized && !ImGui::GetIO().WantCaptureKeyboard)
        {
            if (uMsg == WM_KEYDOWN && wParam == BaseHook::Keys::DetachDll)
            {
                BaseHook::Detach();
                if (auto* app = PluginLoaderApp::Get())
                    app->RequestShutdown();
            }
        }

        // Keep blocking state responsive: update it from the same central policy used in DrawOverlay.
        {
            const HWND foreground = GetForegroundWindow();
            const bool is_focused = (foreground == BaseHook::Data::hWindow);
            const bool menuOpen = BaseHook::Data::bShowMenu;
            const ConsoleMode cm = (PluginLoaderApp::Get() ? PluginLoaderApp::Get()->GetConsole().mode : ConsoleMode::Hidden);

            const auto cap = InputCapture::Compute(is_focused, menuOpen, cm);
            const bool shouldBlock = cap.blockKeyboardWndProc || cap.blockMouseMoveWndProc || cap.blockDirectInputMouseButtons;
            if (BaseHook::Data::bBlockInput != shouldBlock)
                BaseHook::Data::bBlockInput = shouldBlock;
        }

        // If input should be blocked (menu/console open), prevent pure input messages
        // from reaching the game, but ALWAYS forward window-management/system messages
        // so fullscreen exclusive / Alt+Tab / device reset logic can run normally.
        if (BaseHook::Data::bBlockInput)
        {
            switch (uMsg)
            {
            // Keyboard input
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
                if (wParam == VK_F4)
                    break;
                return 1;

            case WM_KEYDOWN:
            case WM_KEYUP:
            case WM_CHAR:

            // Mouse input (mouse buttons/wheel still come from DirectInput for the game)
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

        return BaseHook::Data::oWndProc
            ? CallWindowProc(BaseHook::Data::oWndProc, hWnd, uMsg, wParam, lParam)
            : DefWindowProc(hWnd, uMsg, wParam, lParam);
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

        BaseHook::Data::bImGuiMouseButtonsFromDirectInput = ShouldImGuiMouseButtonsUseDirectInput();

        const auto cap = InputCapture::Compute(is_focused, BaseHook::Data::bShowMenu, cm);

        io.MouseDrawCursor = cap.showMouseCursor;

        const bool shouldBlock = cap.blockKeyboardWndProc || cap.blockMouseMoveWndProc || cap.blockDirectInputMouseButtons;
        if (BaseHook::Data::bBlockInput != shouldBlock)
        {
            BaseHook::Data::bBlockInput = shouldBlock;
            LOG_INFO("Input blocking state changed to: %s", BaseHook::Data::bBlockInput ? "ON" : "OFF");
        }

        // Confine cursor logic: Trap cursor when focused, regardless of menu state or window mode.
        if (is_focused && BaseHook::Data::hWindow)
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


