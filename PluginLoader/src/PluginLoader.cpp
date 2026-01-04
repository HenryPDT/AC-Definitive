#include "PluginLoaderApp.h"

#include "PluginLoaderConfig.h"
#include "core/WindowedMode.h"
#include "core/BaseHook.h"
#include "log.h"

#include <windows.h>

static DWORD WINAPI MainThread(LPVOID lpReserved)
{
    auto hMod = (HMODULE)lpReserved;

    PluginLoaderApp app(hMod);
    app.Init();

    while (!app.IsShutdownRequested())
    {
        app.Tick();
        Sleep(100);
    }

    app.Shutdown();
    FreeLibraryAndExitThread(hMod, 0);
    return 0;
}

BOOL WINAPI DllMain(HMODULE hMod, DWORD dwReason, LPVOID lpReserved)
{
    if (dwReason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hMod);

        Log::Init(hMod);

        // Initialize config early so all hooks can use it
        PluginLoaderConfig::Init(hMod);
        PluginLoaderConfig::Load();
        
        // Pass configuration to BaseHook early
        BaseHook::WindowedMode::EarlyInit(
            PluginLoaderConfig::g_Config.WindowedMode.get(),
            PluginLoaderConfig::g_Config.ResizeBehavior.get(),
            PluginLoaderConfig::g_Config.WindowPosX.get(),
            PluginLoaderConfig::g_Config.WindowPosY.get(),
            PluginLoaderConfig::g_Config.WindowWidth.get(),
            PluginLoaderConfig::g_Config.WindowHeight.get(),
            (int)PluginLoaderConfig::g_Config.DirectXVersion.get(),
            PluginLoaderConfig::g_Config.TargetMonitor.get(),
            (int)PluginLoaderConfig::g_Config.CursorClipMode.get(),
            PluginLoaderConfig::g_Config.OverrideResX.get(),
            PluginLoaderConfig::g_Config.OverrideResY.get(),
            PluginLoaderConfig::g_Config.AlwaysOnTop.get()
        );

        // Install critical hooks immediately to catch window/device creation
        BaseHook::Hooks::InstallEarlyHooks(hMod);
        CreateThread(nullptr, 0, MainThread, hMod, 0, nullptr);
    }
    return TRUE;
}
