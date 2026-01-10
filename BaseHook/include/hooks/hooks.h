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

    // Core Window Procedure Hook
    LRESULT CALLBACK WndProc_Base(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
}