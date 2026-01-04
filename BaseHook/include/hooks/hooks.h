#pragma once
#include <Windows.h>

#include "hooks/DXGIHooks.h"
#include "hooks/DX9Hooks.h"
#include "hooks/InputHooks.h"
#include "hooks/WindowHooks.h"

namespace BaseHook::Hooks
{
    // Global Lifecycle Management
    void InstallEarlyHooks(HMODULE hModule);    // Called from DllMain
    bool Init();                                // Main initialization
    void Shutdown();                            // Full cleanup
    void FinishInitialization();                // Deferred graphics init
}