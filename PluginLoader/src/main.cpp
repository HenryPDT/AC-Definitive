#include "PluginLoaderApp.h"
#include "PluginLoaderConfig.h"
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
        CreateThread(nullptr, 0, MainThread, hMod, 0, nullptr);
    }
    return TRUE;
}