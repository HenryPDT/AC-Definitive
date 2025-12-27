#pragma once
#include <Windows.h>
#include <d3d9.h>
#include <dxgi.h>
#include <XInput.h>

namespace BaseHook
{
    namespace Hooks
    {
        LRESULT CALLBACK WndProc_Base(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

        void FinishInitialization(); // Deferred initialization
        void InstallEarlyHooks(HMODULE hModule);    // Call from DllMain

        // Hook declarations
        HRESULT __stdcall hkEndScene(LPDIRECT3DDEVICE9 pDevice);
        HRESULT __stdcall hkReset(LPDIRECT3DDEVICE9 pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters);
        HRESULT __stdcall hkResetEx(IDirect3DDevice9Ex* pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters, D3DDISPLAYMODEEX* pFullscreenDisplayMode);
        HRESULT __stdcall hkPresent9(LPDIRECT3DDEVICE9 pDevice, CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion);
        HRESULT __stdcall hkTestCooperativeLevel(LPDIRECT3DDEVICE9 pDevice);
        HRESULT __stdcall hkPresentDX11(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
        HRESULT __stdcall hkResizeBuffersDX11(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags);
        HRESULT __stdcall hkResizeTargetDX11(IDXGISwapChain* pSwapChain, const DXGI_MODE_DESC* pNewTargetParameters);
        HRESULT __stdcall hkPresentDX10(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
        HRESULT __stdcall hkResizeBuffersDX10(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags);
        HRESULT __stdcall hkResizeTargetDX10(IDXGISwapChain* pSwapChain, const DXGI_MODE_DESC* pNewTargetParameters);

        void ApplyBufferedInput();
        void InstallWndProcHook();
        void RestoreWndProc();
        void CleanupDirectInput();
        void NotifyDirectInputWindow(HWND hWnd);
        void HandleDeviceChange(WPARAM wParam, LPARAM lParam);
        bool TryGetVirtualXInputState(DWORD dwUserIndex, XINPUT_STATE* pState);

        enum class GamepadInputSource : uint8_t
        {
            None,
            HookedDevice,
            PrivateFallback,
            SonyHID // Unified support for DualSense and DualShock 4
        };

        bool SubmitVirtualGamepadState(GamepadInputSource source, void* context, const XINPUT_STATE& state, bool markAuthoritative, bool* outSourceChanged = nullptr);
        void ResetVirtualGamepad(GamepadInputSource sourceFilter = GamepadInputSource::None, void* contextFilter = nullptr);

        // Window Style Helpers (Bypass hooks)
        DWORD GetTrueWindowStyle(HWND hWnd);
        DWORD GetTrueWindowExStyle(HWND hWnd);
    }
}