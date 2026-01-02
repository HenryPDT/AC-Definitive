#include "pch.h"
#include "base.h"
#include "WindowedMode.h"
#include "hooks/hooks.h"
#include "hooks/DXGI_Common.h"

namespace BaseHook
{
    namespace Hooks
    {
        HRESULT __stdcall hkPresentDX11(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
        {
            return DXGICommon::Present(DXGICommon::Api::D3D11, pSwapChain, SyncInterval, Flags);
        }

        HRESULT __stdcall hkResizeBuffersDX11(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
        {
            // Ensure we have initialized our state/hooks for this swapchain
            DXGICommon::EnsureInitialized(DXGICommon::Api::D3D11, pSwapChain);

            return DXGICommon::ResizeBuffers(DXGICommon::Api::D3D11, pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
        }

        HRESULT __stdcall hkResizeTargetDX11(IDXGISwapChain* pSwapChain, const DXGI_MODE_DESC* pNewTargetParameters)
        {
            return DXGICommon::ResizeTarget(DXGICommon::Api::D3D11, pSwapChain, pNewTargetParameters);
        }
    }
}
