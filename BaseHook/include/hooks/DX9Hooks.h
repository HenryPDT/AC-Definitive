#pragma once
#include <d3d9.h>
#include <d3dx9.h>

namespace BaseHook::Hooks
{
    HRESULT __stdcall hkEndScene(LPDIRECT3DDEVICE9 pDevice);
    HRESULT __stdcall hkReset(LPDIRECT3DDEVICE9 pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters);
    HRESULT __stdcall hkResetEx(IDirect3DDevice9Ex* pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters, D3DDISPLAYMODEEX* pFullscreenDisplayMode);
    HRESULT __stdcall hkPresent9(LPDIRECT3DDEVICE9 pDevice, CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion);
    HRESULT __stdcall hkTestCooperativeLevel(LPDIRECT3DDEVICE9 pDevice);
}
