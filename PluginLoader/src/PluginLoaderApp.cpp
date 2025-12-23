#include "PluginLoaderApp.h"

#include "base.h"
#include "CpuAffinity.h"
#include "PluginLoaderConfig.h"
#include "ImGuiCTX.h"
#include "ImGuiConfigUtils.h"
#include "KeyBind.h"
// #include "WindowedMode.h" // Removed for rebase
#include "crash_handler.h"
#include "log.h"
#include "InputCapture.h"

#include <windows.h>

#include "imgui.h"
#include "imgui_impl_win32.h"

#include "ui/MainMenu.h"

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

    LRESULT __stdcall LoaderWndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
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
            if (PluginLoaderConfig::g_Config.hotkey_ToggleMenu.get().IsPressedEvent(uMsg, wParam))
            {
                BaseHook::Data::bShowMenu = !BaseHook::Data::bShowMenu;
            }
            else if (uMsg == WM_KEYDOWN && wParam == BaseHook::Keys::DetachDll)
            {
                BaseHook::Detach();
                if (auto* app = PluginLoaderApp::Get())
                    app->RequestShutdown();
            }
            else if (PluginLoaderConfig::g_Config.hotkey_ToggleConsole.get().IsPressedEvent(uMsg, wParam))
            {
                if (auto* app = PluginLoaderApp::Get())
                    app->GetConsole().ToggleVisibility();
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
            case WM_KEYDOWN:
            case WM_KEYUP:
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
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
        // Windowed Mode settings removed for rebase
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

        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

        if (auto* app = PluginLoaderApp::Get())
            app->GetConsole().Draw("Console");

        const HWND foreground = GetForegroundWindow();
        const bool is_focused = (foreground == BaseHook::Data::hWindow);
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
    m_loaderInterface.LogToFile = Log::Write;
    m_loaderInterface.LogToConsole = LogUnified;
    m_loaderInterface.RequestUnloadPlugin = PluginLoaderInterface_RequestUnload;
    m_loaderInterface.GetImGuiContext = GetImGuiContext_Impl;
    m_loaderInterface.GetPluginInterface = GetPluginInterface_Impl;

    m_pluginManager.Init(m_module, m_loaderInterface);

    // Auto-apply CPU defaults if not set and game detected
    Game g = m_pluginManager.GetCurrentGame();
    uint64_t systemMask = CpuAffinity::GetSystemAffinityMask();

    if (PluginLoaderConfig::g_Config.CpuAffinityMask == 0) // First run / Not set
    {
        if (g == Game::AC1) {
            LOG_INFO("First Run: Auto-applying AC1 CPU Limit (31 cores).");
            PluginLoaderConfig::g_Config.CpuAffinityMask = 0x7FFFFFFF & systemMask;
        }
        else if (g == Game::AC2 || g == Game::ACB || g == Game::ACR) {
            LOG_INFO("First Run: Auto-applying AC2/Brotherhood/Revelations Core 0 Fix.");
            PluginLoaderConfig::g_Config.CpuAffinityMask = systemMask & ~1ULL;
        }
        else {
            PluginLoaderConfig::g_Config.CpuAffinityMask = systemMask;
        }
        PluginLoaderConfig::Save(); // Save the auto-applied settings immediately
    }

    CpuAffinity::Apply(PluginLoaderConfig::g_Config.CpuAffinityMask);

    m_settings = std::make_unique<LoaderSettings>(LoaderWndProc);
    BaseHook::Start(m_settings.get());
    BaseHook::Data::thisDLLModule = m_module;

    LOG_INFO("Basehook initialized successfully.");
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