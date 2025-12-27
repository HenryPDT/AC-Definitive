#pragma once

#include <dxgi.h>
#include "base.h"

namespace BaseHook::Hooks::DXGICommon
{
    enum class Api
    {
        D3D10,
        D3D11,
    };

    HRESULT Present(Api api, IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
    HRESULT ResizeBuffers(Api api, IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags);
    HRESULT ResizeTarget(Api api, IDXGISwapChain* pSwapChain, const DXGI_MODE_DESC* pNewTargetParameters);

    bool EnsureInitialized(Api api, IDXGISwapChain* pSwapChain);
    void EnsureSwapChainFullscreenHook(IDXGISwapChain* pSwapChain);
    void DisableDXGIAltEnter(IDXGISwapChain* pSwapChain);
}


