#pragma once
#include <Windows.h>

namespace BaseHook::Hooks
{
    void InstallWndProcHook();
    void RestoreWndProc();
    LRESULT CALLBACK WndProc_Base(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    
    // Window Style Helpers (Bypass hooks)
    DWORD GetTrueWindowStyle(HWND hWnd);
    DWORD GetTrueWindowExStyle(HWND hWnd);
    
    void HandleDeviceChange(WPARAM wParam, LPARAM lParam);
}
