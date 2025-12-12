#include "base.h"
#include "PluginManager.h"
#include "ImGuiConsole.h"
#include "log.h"
#include "crash_handler.h"
#include <windows.h>
#include <d3d9.h>
#include "imgui_impl_win32.h"
#include "imgui_impl_dx9.h"
#include <memory>
#include <vector>

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace Globals
{
    HMODULE           hModule = NULL;
    bool              bShutdown = false;
    PluginManager     pluginManager;
    ImGuiConsole      console;
    PluginLoaderInterface loaderInterface;
}

// Sink that only pushes to the on-screen console.
void LogConsoleSink(const char* text)
{
    Globals::console.AddLogF("%s", text);
}

// Unified entrypoint for plugins: writes once, fans out to file + console sinks.
void LogUnified(const char* text)
{
    Log::Write("%s", text);
}

ImGuiContext* GetImGuiContext_Impl()
{
    return ImGui::GetCurrentContext();
}

Game GetCurrentGame_Impl() { return Globals::pluginManager.GetCurrentGame(); }
void PluginLoaderInterface_RequestUnload(HMODULE pluginHandle) { /* Not implemented for now */ }

class MySettings : public BaseHook::Settings
{
public:
    MySettings(WndProc_t wndProc) : BaseHook::Settings(wndProc, true) {}
    virtual void OnActivate() override {}
    virtual void OnDetach() override {}

    virtual void DrawOverlay() override
    {
        ImGuiIO& io = ImGui::GetIO();

        if (Globals::loaderInterface.m_ImGuiContext == nullptr)
            Globals::loaderInterface.m_ImGuiContext = ImGui::GetCurrentContext();

        // Only block input when our own UI is explicitly open (menu or foreground console),
        // and only while our window is actually focused. This prevents alt-tabbing or
        // background focus changes from leaving the game permanently input-blocked.
        HWND foreground = GetForegroundWindow();
        const bool is_focused = (foreground == BaseHook::Data::hWindow);

        bool bShouldBlock = is_focused &&
            (BaseHook::Data::bShowMenu || Globals::console.mode == ConsoleMode::ForegroundAndFocusable);

        io.MouseDrawCursor = bShouldBlock;

        // Update the global flag for DirectInput hooks and the WndProc master block
        if (BaseHook::Data::bBlockInput != bShouldBlock)
        {
            BaseHook::Data::bBlockInput = bShouldBlock;
            LOG_INFO("Input blocking state changed to: %s", BaseHook::Data::bBlockInput ? "ON" : "OFF");
        }

        // Always confine the cursor to the game window client area while the window is focused.
        // This prevents the mouse from drifting to other monitors or clicking the desktop/background,
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

        Globals::pluginManager.UpdatePlugins();
        Globals::console.Draw("Console");
    }

    virtual void DrawMenu() override
    {
        ImGui::ShowDemoWindow(&BaseHook::Data::bShowMenu);
        
        // if (ImGui::Begin("Plugin Loader", &BaseHook::Data::bShowMenu))
        // {
        //     const char* gameNames[] = { "Unknown", "AC1", "AC2", "ACB", "ACR" };
        //     int gameIdx = (int)Globals::pluginManager.GetCurrentGame();
        //     if (gameIdx < 0 || gameIdx > 4) gameIdx = 0;
        //     ImGui::Text("Detected Game: %s", gameNames[gameIdx]);
        //     ImGui::Separator();
        //     Globals::pluginManager.DrawPluginMenu();
        // }
        // ImGui::End();
        // Globals::pluginManager.RenderPluginMenus();
    }
};

LRESULT __stdcall WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    // We are feeding mouse input via DirectInput. To prevent double input, we must
    // stop ImGui's Win32 backend from processing mouse messages.
    // FIX: ImGui needs to see mouse messages to handle its internal state correctly.
    // We should not filter them here; we only block the game from receiving them later.

    if (BaseHook::Data::bIsInitialized)
        ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);

    // Handle our own hotkeys, but only if an ImGui window doesn't want keyboard input.
    if (BaseHook::Data::bIsInitialized && !ImGui::GetIO().WantCaptureKeyboard)
    {
        if (uMsg == WM_KEYDOWN)
        {
            if (wParam == BaseHook::Keys::ToggleMenu)
            {
                BaseHook::Data::bShowMenu = !BaseHook::Data::bShowMenu;
            }
            else if (wParam == BaseHook::Keys::DetachDll)
            {
                BaseHook::Detach();
                Globals::bShutdown = true;
            }
            else if (wParam == VK_OEM_3) // Tilde key
            {
                Globals::console.ToggleVisibility();
            }
        }
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

        // Mouse input (we don't pass these to ImGui's Win32 backend, but we do
        // want to stop the game from seeing them while the UI is active).
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

        // Everything else (activation, focus, sizing, system commands, etc.)
        // must always reach the original window procedure.
        default:
            break;
        }
    }

    // If we've reached here, the message should be passed to the game.
    return CallWindowProc(BaseHook::Data::oWndProc, hWnd, uMsg, wParam, lParam);
}



void Shutdown()
{
    LOG_INFO("Shutdown initiated.");
    Globals::pluginManager.ShutdownPlugins();
    BaseHook::Detach();
    Log::RemoveSink(LogConsoleSink);
    CrashHandler::Shutdown();
    Log::Shutdown();
}

DWORD WINAPI MainThread(LPVOID lpReserved)
{
    Globals::hModule = (HMODULE)lpReserved;
    Log::Init(Globals::hModule);
    Log::AddSink(LogConsoleSink);
    CrashHandler::Init();

    LOG_INFO("Plugin Loader Attached.");

    Globals::loaderInterface.GetCurrentGame = GetCurrentGame_Impl;
    Globals::loaderInterface.LogToFile = Log::Write;
    Globals::loaderInterface.LogToConsole = LogUnified;
    Globals::loaderInterface.RequestUnloadPlugin = PluginLoaderInterface_RequestUnload;
    Globals::loaderInterface.GetImGuiContext = GetImGuiContext_Impl;

    Globals::pluginManager.Init(Globals::hModule, Globals::loaderInterface);

    static MySettings settings(WndProc);
    BaseHook::Start(&settings);
    BaseHook::Data::thisDLLModule = Globals::hModule;

    LOG_INFO("Basehook initialized successfully.");

    while (!Globals::bShutdown)
    {
        Sleep(100);
    }

    Shutdown();

    FreeLibraryAndExitThread(Globals::hModule, 0);
    return 0;
}

BOOL WINAPI DllMain(HMODULE hMod, DWORD dwReason, LPVOID lpReserved)
{
    if (dwReason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hMod);
        CreateThread(nullptr, 0, MainThread, hMod, 0, nullptr);
    }
    return TRUE;
}
