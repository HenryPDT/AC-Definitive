#include "pch.h"
#include "base.h"
#include "DXGI_Common.h"

namespace BaseHook
{
    namespace Hooks
    {
        HRESULT __stdcall hkPresentDX10(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
        {
            return DXGICommon::Present(DXGICommon::Api::D3D10, pSwapChain, SyncInterval, Flags);
        }

        HRESULT __stdcall hkResizeBuffersDX10(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
        {
            return DXGICommon::ResizeBuffers(DXGICommon::Api::D3D10, pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
        }

        HRESULT __stdcall hkResizeTargetDX10(IDXGISwapChain* pSwapChain, const DXGI_MODE_DESC* pNewTargetParameters)
        {
            return DXGICommon::ResizeTarget(DXGICommon::Api::D3D10, pSwapChain, pNewTargetParameters);
        }
    }
}
