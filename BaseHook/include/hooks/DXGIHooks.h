#pragma once

#include <dxgi.h>


namespace BaseHook::Hooks
{
    enum class Api
    {
        D3D10,
        D3D11,
    };

    HRESULT Present(Api api, IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
    HRESULT ResizeBuffers(Api api, IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags);
    HRESULT ResizeTarget(Api api, IDXGISwapChain* pSwapChain, const DXGI_MODE_DESC* pNewTargetParameters);

    // Specific version hooks for D3DCreateHooks
    HRESULT __stdcall hkPresentDX11(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
    HRESULT __stdcall hkResizeBuffersDX11(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags);
    HRESULT __stdcall hkResizeTargetDX11(IDXGISwapChain* pSwapChain, const DXGI_MODE_DESC* pNewTargetParameters);
    
    HRESULT __stdcall hkPresentDX10(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
    HRESULT __stdcall hkResizeBuffersDX10(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags);
    HRESULT __stdcall hkResizeTargetDX10(IDXGISwapChain* pSwapChain, const DXGI_MODE_DESC* pNewTargetParameters);

    bool EnsureInitialized(Api api, IDXGISwapChain* pSwapChain);
    void EnsureSwapChainFullscreenHook(IDXGISwapChain* pSwapChain);
    void DisableDXGIAltEnter(IDXGISwapChain* pSwapChain);
}
