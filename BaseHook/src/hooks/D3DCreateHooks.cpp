#include "pch.h"

#include "hooks/D3DCreateHooks.h"
#include "hooks/DXGI_Common.h"

#include "base.h"
#include "WindowedMode.h"
#include "log.h"
#include "ComPtr.h"
#include <d3d9.h>

namespace BaseHook
{
    namespace Hooks
    {
        // --- D3D/DXGI Creation Typedefs ---
        typedef IDirect3D9* (WINAPI* Direct3DCreate9_t)(UINT);
        typedef HRESULT(WINAPI* Direct3DCreate9Ex_t)(UINT, IDirect3D9Ex**);
        typedef HRESULT(WINAPI* CreateDXGIFactory_t)(REFIID, void**);
        typedef HRESULT(WINAPI* CreateDXGIFactory1_t)(REFIID, void**);
        typedef HRESULT(WINAPI* CreateDXGIFactory2_t)(UINT, REFIID, void**);
        typedef HRESULT(STDMETHODCALLTYPE* IDirect3D9_CreateDevice_t)(IDirect3D9*, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS*, IDirect3DDevice9**);
        typedef HRESULT(STDMETHODCALLTYPE* IDirect3D9Ex_CreateDeviceEx_t)(IDirect3D9Ex*, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS*, D3DDISPLAYMODEEX*, IDirect3DDevice9Ex**);
        typedef HRESULT(STDMETHODCALLTYPE* IDXGIFactory_CreateSwapChain_t)(IDXGIFactory*, IUnknown*, DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**);
        typedef HRESULT(STDMETHODCALLTYPE* IDXGIFactory2_CreateSwapChainForHwnd_t)(IDXGIFactory2*, IUnknown*, HWND, const DXGI_SWAP_CHAIN_DESC1*, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*, IDXGIOutput*, IDXGISwapChain1**);
        typedef HRESULT(STDMETHODCALLTYPE* IDXGIFactory_MakeWindowAssociation_t)(IDXGIFactory*, HWND, UINT);
        
        typedef HRESULT(WINAPI* D3D10CreateDeviceAndSwapChain_t)(IDXGIAdapter*, D3D10_DRIVER_TYPE, HMODULE, UINT, UINT, DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**, ID3D10Device**);
        typedef HRESULT(WINAPI* D3D11CreateDeviceAndSwapChain_t)(IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT, const D3D_FEATURE_LEVEL*, UINT, UINT, const DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**, ID3D11Device**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext**);
        typedef HRESULT(WINAPI* D3D11CreateDevice_t)(IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT, const D3D_FEATURE_LEVEL*, UINT, UINT, ID3D11Device**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext**);

        static Direct3DCreate9_t oDirect3DCreate9 = nullptr;
        static Direct3DCreate9Ex_t oDirect3DCreate9Ex = nullptr;
        
        static CreateDXGIFactory_t oCreateDXGIFactory = nullptr;
        static CreateDXGIFactory1_t oCreateDXGIFactory1 = nullptr;
        static CreateDXGIFactory2_t oCreateDXGIFactory2 = nullptr;

        static IDirect3D9_CreateDevice_t oIDirect3D9_CreateDevice = nullptr;
        static IDirect3D9Ex_CreateDeviceEx_t oIDirect3D9Ex_CreateDeviceEx = nullptr;
        static IDXGIFactory_CreateSwapChain_t oIDXGIFactory_CreateSwapChain = nullptr;
        static IDXGIFactory2_CreateSwapChainForHwnd_t oIDXGIFactory2_CreateSwapChainForHwnd = nullptr;
        static IDXGIFactory_MakeWindowAssociation_t oIDXGIFactory_MakeWindowAssociation = nullptr;

        static D3D10CreateDeviceAndSwapChain_t oD3D10CreateDeviceAndSwapChain = nullptr;
        static D3D11CreateDeviceAndSwapChain_t oD3D11CreateDeviceAndSwapChain = nullptr;
        static D3D11CreateDevice_t oD3D11CreateDevice = nullptr;

        static void AdjustSwapChainDesc(DXGI_SWAP_CHAIN_DESC* pDesc)
        {
            if (!pDesc) return;

            if (!WindowedMode::ShouldHandle() && !WindowedMode::ShouldApplyResolutionOverride()) return;

            if (WindowedMode::ShouldHandle())
                LOG_INFO("AdjustSwapChainDesc: Forced Windowed");
            else
                LOG_INFO("AdjustSwapChainDesc: Resolution Override (Exclusive)");

            if (pDesc->BufferDesc.Width > 0 && pDesc->BufferDesc.Height > 0) {
                WindowedMode::NotifyResolutionChange(pDesc->BufferDesc.Width, pDesc->BufferDesc.Height);
            }
            if (WindowedMode::g_State.activeMode == WindowedMode::Mode::BorderlessFullscreen) {
                int screenW = GetSystemMetrics(SM_CXSCREEN);
                int screenH = GetSystemMetrics(SM_CYSCREEN);
                pDesc->BufferDesc.Width = screenW;
                pDesc->BufferDesc.Height = screenH;
                WindowedMode::NotifyResolutionChange(screenW, screenH);
            }

            if (WindowedMode::g_State.overrideWidth > 0 && WindowedMode::g_State.overrideHeight > 0)
            {
                pDesc->BufferDesc.Width = WindowedMode::g_State.overrideWidth;
                pDesc->BufferDesc.Height = WindowedMode::g_State.overrideHeight;
                pDesc->BufferDesc.RefreshRate.Numerator = 0;
                pDesc->BufferDesc.RefreshRate.Denominator = 0;
            }

            if (WindowedMode::ShouldHandle())
            {
                pDesc->Windowed = TRUE;
                pDesc->BufferDesc.RefreshRate.Numerator = 0;
                pDesc->BufferDesc.RefreshRate.Denominator = 0;
                pDesc->BufferDesc.Scaling = DXGI_MODE_SCALING_STRETCHED;

                // Flip models (DXGI_SWAP_EFFECT_FLIP_*) require BufferCount >= 2.
                if ((pDesc->SwapEffect == DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL || pDesc->SwapEffect == DXGI_SWAP_EFFECT_FLIP_DISCARD) && 
                    pDesc->BufferCount < 2)
                {
                    LOG_INFO("AdjustSwapChainDesc: Bumping BufferCount from %d to 2 for Flip Model compatibility.", pDesc->BufferCount);
                    pDesc->BufferCount = 2;
                }
            }

            if (Data::hWindow == NULL && pDesc->OutputWindow != NULL) {
                Data::hWindow = pDesc->OutputWindow;
            }
        }

        static void AdjustSwapChainDesc1(DXGI_SWAP_CHAIN_DESC1* pDesc, DXGI_SWAP_CHAIN_FULLSCREEN_DESC** ppFSDesc)
        {
            if (!pDesc) return;

            if (WindowedMode::ShouldHandle())
            {
                LOG_INFO("AdjustSwapChainDesc1: Forced Windowed");
                
                // Force windowed by removing the fullscreen descriptor
                // CreateSwapChainForHwnd fails if you pass a FSDesc with Windowed=TRUE (it expects Windowed=FALSE for that struct)
                // Passing nullptr means Windowed.
                if (*ppFSDesc)
                {
                    LOG_INFO("AdjustSwapChainDesc1: Removing Fullscreen Descriptor to force Windowed.");
                    *ppFSDesc = nullptr;
                }

                pDesc->Scaling = DXGI_SCALING_STRETCH;

                // Flip model check
                if ((pDesc->SwapEffect == DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL || pDesc->SwapEffect == DXGI_SWAP_EFFECT_FLIP_DISCARD) && 
                    pDesc->BufferCount < 2)
                {
                    LOG_INFO("AdjustSwapChainDesc1: Bumping BufferCount from %d to 2 for Flip Model compatibility.", pDesc->BufferCount);
                    pDesc->BufferCount = 2;
                }
            }
            
            if (WindowedMode::ShouldHandle() || WindowedMode::ShouldApplyResolutionOverride())
            {
                 if (pDesc->Width > 0 && pDesc->Height > 0)
                     WindowedMode::NotifyResolutionChange(pDesc->Width, pDesc->Height);

                 if (WindowedMode::g_State.activeMode == WindowedMode::Mode::BorderlessFullscreen) {
                    int screenW = GetSystemMetrics(SM_CXSCREEN);
                    int screenH = GetSystemMetrics(SM_CYSCREEN);
                    pDesc->Width = screenW;
                    pDesc->Height = screenH;
                    WindowedMode::NotifyResolutionChange(screenW, screenH);
                 }
                 else if (WindowedMode::g_State.overrideWidth > 0 && WindowedMode::g_State.overrideHeight > 0)
                 {
                     pDesc->Width = WindowedMode::g_State.overrideWidth;
                     pDesc->Height = WindowedMode::g_State.overrideHeight;
                 }
            }
        }

        static HRESULT WINAPI hkD3D10CreateDeviceAndSwapChain(IDXGIAdapter* pAdapter, D3D10_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, UINT SDKVersion, DXGI_SWAP_CHAIN_DESC* pSwapChainDesc, IDXGISwapChain** ppSwapChain, ID3D10Device** ppDevice)
        {
            WindowedMode::CheckAndApplyPendingState();

            DXGI_SWAP_CHAIN_DESC desc;
            if (pSwapChainDesc) desc = *pSwapChainDesc;
            DXGI_SWAP_CHAIN_DESC* pDescToUse = pSwapChainDesc;

            if ((WindowedMode::ShouldHandle() || WindowedMode::ShouldApplyResolutionOverride()) && pSwapChainDesc) {
                AdjustSwapChainDesc(&desc);
                pDescToUse = &desc;
            }

            if (pDescToUse && pDescToUse->OutputWindow) {
                if (pDescToUse->OutputWindow != Data::hWindow) {
                    if (Data::hWindow) RestoreWndProc();
                    Data::hWindow = pDescToUse->OutputWindow;
                    InstallWndProcHook();
                }

                WindowedMode::SetManagedWindow(pDescToUse->OutputWindow);
                if (WindowedMode::ShouldHandle())
                    WindowedMode::Apply(pDescToUse->OutputWindow);
            }

            HRESULT hr = oD3D10CreateDeviceAndSwapChain(pAdapter, DriverType, Software, Flags, SDKVersion, pDescToUse, ppSwapChain, ppDevice);
            
            if (SUCCEEDED(hr) && ppSwapChain && *ppSwapChain)
            {
                DXGI_SWAP_CHAIN_DESC desc;
                if (SUCCEEDED((*ppSwapChain)->GetDesc(&desc)))
                {
                    if (desc.BufferDesc.Width > 0 && desc.BufferDesc.Height > 0)
                        WindowedMode::NotifyResolutionChange(desc.BufferDesc.Width, desc.BufferDesc.Height);
                }

                DXGICommon::EnsureSwapChainFullscreenHook(*ppSwapChain);
                DXGICommon::DisableDXGIAltEnter(*ppSwapChain);
            }

            return hr;
        }

        static HRESULT WINAPI hkD3D11CreateDevice(IDXGIAdapter* pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, const D3D_FEATURE_LEVEL* pFeatureLevels, UINT FeatureLevels, UINT SDKVersion, ID3D11Device** ppDevice, D3D_FEATURE_LEVEL* pFeatureLevel, ID3D11DeviceContext** ppImmediateContext)
        {
            return oD3D11CreateDevice(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion, ppDevice, pFeatureLevel, ppImmediateContext);
        }

        static HRESULT WINAPI hkD3D11CreateDeviceAndSwapChain(IDXGIAdapter* pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, const D3D_FEATURE_LEVEL* pFeatureLevels, UINT FeatureLevels, UINT SDKVersion, const DXGI_SWAP_CHAIN_DESC* pSwapChainDesc, IDXGISwapChain** ppSwapChain, ID3D11Device** ppDevice, D3D_FEATURE_LEVEL* pFeatureLevel, ID3D11DeviceContext** ppImmediateContext)
        {
            WindowedMode::CheckAndApplyPendingState();

            DXGI_SWAP_CHAIN_DESC desc;
            if (pSwapChainDesc) desc = *pSwapChainDesc;
            DXGI_SWAP_CHAIN_DESC* pDescToUse = (pSwapChainDesc != nullptr) ? &desc : nullptr;

            if ((WindowedMode::ShouldHandle() || WindowedMode::ShouldApplyResolutionOverride()) && pSwapChainDesc) {
                AdjustSwapChainDesc(&desc);
            }

            if (pDescToUse && pDescToUse->OutputWindow) { 
                if (pDescToUse->OutputWindow != Data::hWindow) {
                    if (Data::hWindow) RestoreWndProc();
                    Data::hWindow = pDescToUse->OutputWindow;
                    InstallWndProcHook();
                }

                WindowedMode::SetManagedWindow(pDescToUse->OutputWindow);
                if (WindowedMode::ShouldHandle())
                    WindowedMode::Apply(pDescToUse->OutputWindow);
            }

            HRESULT hr = oD3D11CreateDeviceAndSwapChain(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion, pDescToUse, ppSwapChain, ppDevice, pFeatureLevel, ppImmediateContext);
            
            if (SUCCEEDED(hr) && ppSwapChain && *ppSwapChain)
            {
                DXGI_SWAP_CHAIN_DESC desc;
                if (SUCCEEDED((*ppSwapChain)->GetDesc(&desc)))
                {
                    if (desc.BufferDesc.Width > 0 && desc.BufferDesc.Height > 0)
                        WindowedMode::NotifyResolutionChange(desc.BufferDesc.Width, desc.BufferDesc.Height);
                }

                DXGICommon::EnsureSwapChainFullscreenHook(*ppSwapChain);
                DXGICommon::DisableDXGIAltEnter(*ppSwapChain);
            }

            return hr;
        }

        static HRESULT STDMETHODCALLTYPE hkIDirect3D9_CreateDevice(IDirect3D9* pD3D, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice9** ppReturnedDeviceInterface)
        {
            WindowedMode::CheckAndApplyPendingState();

            if (pPresentationParameters)
            {
                WindowedMode::UpdateDetectedStateDX9(!pPresentationParameters->Windowed);
            }

            D3DPRESENT_PARAMETERS params;
            if (pPresentationParameters) params = *pPresentationParameters;
            D3DPRESENT_PARAMETERS* pParamsToUse = pPresentationParameters;

            if ((WindowedMode::ShouldHandle() || WindowedMode::ShouldApplyResolutionOverride()) && pPresentationParameters) {
                if (WindowedMode::ShouldHandle())
                    LOG_INFO("D3D9 CreateDevice Spoof: Forced Windowed");
                else
                    LOG_INFO("D3D9 CreateDevice Spoof: Resolution Override (Exclusive)");

                if (WindowedMode::g_State.overrideWidth > 0 && WindowedMode::g_State.overrideHeight > 0)
                {
                    params.BackBufferWidth = WindowedMode::g_State.overrideWidth;
                    params.BackBufferHeight = WindowedMode::g_State.overrideHeight;
                }

                if (params.BackBufferWidth > 0 && params.BackBufferHeight > 0) {
                    WindowedMode::NotifyResolutionChange(params.BackBufferWidth, params.BackBufferHeight);
                }

                if (WindowedMode::ShouldHandle()) {
                    params.Windowed = TRUE;
                    params.FullScreen_RefreshRateInHz = 0;
                }
                
                // --- FIX FOR PROXY WINDOWS (AC1) ---
                if (params.hDeviceWindow != NULL && hFocusWindow != NULL && params.hDeviceWindow != hFocusWindow)
                {
                    if (WindowedMode::ShouldHandle())
                    {
                        LOG_INFO("D3D9 CreateDevice: Overriding hDeviceWindow (%p -> %p) to prevent Proxy Window usage.", params.hDeviceWindow, hFocusWindow);
                        params.hDeviceWindow = hFocusWindow;
                    }
                }
                else if (params.hDeviceWindow != NULL && Data::hWindow != NULL && params.hDeviceWindow != Data::hWindow)
                {
                     if (WindowedMode::ShouldHandle())
                    {
                        LOG_INFO("D3D9 CreateDevice: Overriding hDeviceWindow (%p -> %p) to prevent Proxy Window usage.", params.hDeviceWindow, Data::hWindow);
                        params.hDeviceWindow = Data::hWindow;
                    }
                }

                if (Data::hWindow != NULL && hFocusWindow != Data::hWindow)
                {
                    if (WindowedMode::ShouldHandle())
                    {
                         hFocusWindow = Data::hWindow;
                    }
                }

                pParamsToUse = &params;

                if (Data::hWindow) {
                    WindowedMode::SetManagedWindow(Data::hWindow);
                    WindowedMode::Apply(Data::hWindow);
                }
            }
            HRESULT hr = oIDirect3D9_CreateDevice(pD3D, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pParamsToUse, ppReturnedDeviceInterface);
            
            if (SUCCEEDED(hr) && ppReturnedDeviceInterface && *ppReturnedDeviceInterface)
            {
                IDirect3DSurface9* pBackBuffer = nullptr;
                if (SUCCEEDED((*ppReturnedDeviceInterface)->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer))) {
                    D3DSURFACE_DESC desc;
                    pBackBuffer->GetDesc(&desc);
                    pBackBuffer->Release();
                    WindowedMode::NotifyResolutionChange(desc.Width, desc.Height);
                }
            }

            return hr;
        }

        static HRESULT STDMETHODCALLTYPE hkIDirect3D9Ex_CreateDeviceEx(IDirect3D9Ex* pD3D, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, D3DDISPLAYMODEEX* pFullscreenDisplayMode, IDirect3DDevice9Ex** ppReturnedDeviceInterface)
        {
            WindowedMode::CheckAndApplyPendingState();

            if (pPresentationParameters)
            {
                WindowedMode::UpdateDetectedStateDX9(!pPresentationParameters->Windowed);
            }

            D3DPRESENT_PARAMETERS params;
            if (pPresentationParameters) params = *pPresentationParameters;
            D3DPRESENT_PARAMETERS* pParamsToUse = pPresentationParameters;
            D3DDISPLAYMODEEX* pFullscreenModeToUse = pFullscreenDisplayMode;

            if ((WindowedMode::ShouldHandle() || WindowedMode::ShouldApplyResolutionOverride()) && pPresentationParameters) {
                if (WindowedMode::ShouldHandle())
                    LOG_INFO("D3D9Ex CreateDeviceEx Spoof: Forced Windowed");
                else
                    LOG_INFO("D3D9Ex CreateDeviceEx Spoof: Resolution Override (Exclusive)");

                if (WindowedMode::g_State.overrideWidth > 0 && WindowedMode::g_State.overrideHeight > 0)
                {
                    params.BackBufferWidth = WindowedMode::g_State.overrideWidth;
                    params.BackBufferHeight = WindowedMode::g_State.overrideHeight;
                }

                if (params.BackBufferWidth > 0 && params.BackBufferHeight > 0) {
                    WindowedMode::NotifyResolutionChange(params.BackBufferWidth, params.BackBufferHeight);
                }

                if (WindowedMode::ShouldHandle()) {
                    params.Windowed = TRUE;
                    params.FullScreen_RefreshRateInHz = 0;
                    pFullscreenModeToUse = nullptr;
                }
                
                // --- FIX FOR PROXY WINDOWS (AC1) ---
                if (params.hDeviceWindow != NULL && hFocusWindow != NULL && params.hDeviceWindow != hFocusWindow)
                {
                    if (WindowedMode::ShouldHandle())
                    {
                        LOG_INFO("D3D9Ex CreateDeviceEx: Overriding hDeviceWindow (%p -> %p) to prevent Proxy Window usage.", params.hDeviceWindow, hFocusWindow);
                        params.hDeviceWindow = hFocusWindow;
                    }
                }
                else if (params.hDeviceWindow != NULL && Data::hWindow != NULL && params.hDeviceWindow != Data::hWindow)
                {
                     if (WindowedMode::ShouldHandle())
                    {
                        LOG_INFO("D3D9Ex CreateDeviceEx: Overriding hDeviceWindow (%p -> %p) to prevent Proxy Window usage.", params.hDeviceWindow, Data::hWindow);
                        params.hDeviceWindow = Data::hWindow;
                    }
                }

                if (Data::hWindow != NULL && hFocusWindow != Data::hWindow)
                {
                    if (WindowedMode::ShouldHandle())
                    {
                         hFocusWindow = Data::hWindow;
                    }
                }

                pParamsToUse = &params;

                if (Data::hWindow) {
                    WindowedMode::SetManagedWindow(Data::hWindow);
                    WindowedMode::Apply(Data::hWindow);
                }
            }

            HRESULT hr = oIDirect3D9Ex_CreateDeviceEx(pD3D, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pParamsToUse, pFullscreenModeToUse, ppReturnedDeviceInterface);

            if (SUCCEEDED(hr) && ppReturnedDeviceInterface && *ppReturnedDeviceInterface)
            {
                IDirect3DSurface9* pBackBuffer = nullptr;
                if (SUCCEEDED((*ppReturnedDeviceInterface)->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer))) {
                    D3DSURFACE_DESC desc;
                    pBackBuffer->GetDesc(&desc);
                    pBackBuffer->Release();
                    WindowedMode::NotifyResolutionChange(desc.Width, desc.Height);
                }
                
                if (!Data::oResetEx)
                {
                    void** vtable = *(void***)*ppReturnedDeviceInterface;
                    if (MH_CreateHook(vtable[132], hkResetEx, (LPVOID*)&Data::oResetEx) == MH_OK)
                        MH_EnableHook(vtable[132]);
                }
            }

            return hr;
        }

        static IDirect3D9* WINAPI hkDirect3DCreate9(UINT SDKVersion)
        {
            if (!oDirect3DCreate9) return NULL;
            IDirect3D9* pD3D = oDirect3DCreate9(SDKVersion);
            if (pD3D && !oIDirect3D9_CreateDevice) {
                void** vtable = *(void***)pD3D;
                if (MH_CreateHook(vtable[16], hkIDirect3D9_CreateDevice, (LPVOID*)&oIDirect3D9_CreateDevice) == MH_OK)
                    MH_EnableHook(vtable[16]);
            }
            return pD3D;
        }

        static HRESULT WINAPI hkDirect3DCreate9Ex(UINT SDKVersion, IDirect3D9Ex** ppD3D)
        {
            if (!oDirect3DCreate9Ex) return E_FAIL;
            HRESULT hr = oDirect3DCreate9Ex(SDKVersion, ppD3D);
            if (SUCCEEDED(hr) && ppD3D && *ppD3D) {
                void** vtable = *(void***)*ppD3D;
                if (!oIDirect3D9_CreateDevice && MH_CreateHook(vtable[16], hkIDirect3D9_CreateDevice, (LPVOID*)&oIDirect3D9_CreateDevice) == MH_OK)
                    MH_EnableHook(vtable[16]);
                if (!oIDirect3D9Ex_CreateDeviceEx && MH_CreateHook(vtable[20], hkIDirect3D9Ex_CreateDeviceEx, (LPVOID*)&oIDirect3D9Ex_CreateDeviceEx) == MH_OK)
                    MH_EnableHook(vtable[20]);
            }
            return hr;
        }

        static HRESULT STDMETHODCALLTYPE hkIDXGIFactory_CreateSwapChain(IDXGIFactory* pFactory, IUnknown* pDevice, DXGI_SWAP_CHAIN_DESC* pDesc, IDXGISwapChain** ppSwapChain)
        {
            WindowedMode::CheckAndApplyPendingState();

            DXGI_SWAP_CHAIN_DESC desc;
            if (pDesc) desc = *pDesc;
            DXGI_SWAP_CHAIN_DESC* pDescToUse = pDesc;

            if ((WindowedMode::ShouldHandle() || WindowedMode::ShouldApplyResolutionOverride()) && pDesc) {
                AdjustSwapChainDesc(&desc);
                pDescToUse = &desc;
            }

            if (!oIDXGIFactory_CreateSwapChain) return E_FAIL;
            HRESULT hr = oIDXGIFactory_CreateSwapChain(pFactory, pDevice, pDescToUse, ppSwapChain);

            if (SUCCEEDED(hr) && ppSwapChain && *ppSwapChain)
            {
                DXGI_SWAP_CHAIN_DESC desc;
                if (SUCCEEDED((*ppSwapChain)->GetDesc(&desc)))
                {
                    if (desc.BufferDesc.Width > 0 && desc.BufferDesc.Height > 0)
                        WindowedMode::NotifyResolutionChange(desc.BufferDesc.Width, desc.BufferDesc.Height);
                }

                if (pDescToUse && pDescToUse->OutputWindow)
                    WindowedMode::SetManagedWindow(pDescToUse->OutputWindow);

                DXGICommon::EnsureSwapChainFullscreenHook(*ppSwapChain);
                DXGICommon::DisableDXGIAltEnter(*ppSwapChain);
            }

            return hr;
        }

        static HRESULT STDMETHODCALLTYPE hkIDXGIFactory2_CreateSwapChainForHwnd(IDXGIFactory2* pFactory, IUnknown* pDevice, HWND hWnd, const DXGI_SWAP_CHAIN_DESC1* pDesc, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pFullscreenDesc, IDXGIOutput* pRestrictToOutput, IDXGISwapChain1** ppSwapChain)
        {
            WindowedMode::CheckAndApplyPendingState();

            DXGI_SWAP_CHAIN_DESC1 desc1;
            if (pDesc) desc1 = *pDesc;
            
            DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsDesc;
            if (pFullscreenDesc) fsDesc = *pFullscreenDesc;
            
            DXGI_SWAP_CHAIN_DESC1* pDescToUse = pDesc ? &desc1 : nullptr;
            DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pFSDescToUse = pFullscreenDesc ? &fsDesc : nullptr;

            if (WindowedMode::ShouldHandle() || WindowedMode::ShouldApplyResolutionOverride()) {
                AdjustSwapChainDesc1(&desc1, &pFSDescToUse);
            }

            if (hWnd && Data::hWindow == NULL) Data::hWindow = hWnd;
            
            if (WindowedMode::ShouldHandle()) {
                if (hWnd) {
                    WindowedMode::SetManagedWindow(hWnd);
                    WindowedMode::Apply(hWnd);
                }
            }

            HRESULT hr = oIDXGIFactory2_CreateSwapChainForHwnd(pFactory, pDevice, hWnd, pDescToUse, pFSDescToUse, pRestrictToOutput, ppSwapChain);

            if (SUCCEEDED(hr) && ppSwapChain && *ppSwapChain)
            {
                DXGI_SWAP_CHAIN_DESC desc;
                if (SUCCEEDED((*ppSwapChain)->GetDesc(&desc)))
                {
                    if (desc.BufferDesc.Width > 0 && desc.BufferDesc.Height > 0)
                        WindowedMode::NotifyResolutionChange(desc.BufferDesc.Width, desc.BufferDesc.Height);
                }

                DXGICommon::EnsureSwapChainFullscreenHook(*ppSwapChain);
                DXGICommon::DisableDXGIAltEnter(*ppSwapChain);
            }
            return hr;
        }

        static HRESULT STDMETHODCALLTYPE hkIDXGIFactory_MakeWindowAssociation(IDXGIFactory* pFactory, HWND WindowHandle, UINT Flags)
        {
            // If we are handling the window, we force NO_ALT_ENTER to prevent DXGI from messing with our window state.
            if (WindowedMode::ShouldHandle())
            {
                Flags |= DXGI_MWA_NO_ALT_ENTER;
            }
            return oIDXGIFactory_MakeWindowAssociation(pFactory, WindowHandle, Flags);
        }

        static void InstallFactoryHooks(IUnknown* pFactory)
        {
            if (!pFactory) return;
            void** vtable = *(void***)pFactory;

            // Hook MakeWindowAssociation (Index 8)
            if (!oIDXGIFactory_MakeWindowAssociation) {
                if (MH_CreateHook(vtable[8], hkIDXGIFactory_MakeWindowAssociation, (LPVOID*)&oIDXGIFactory_MakeWindowAssociation) == MH_OK) {
                    MH_EnableHook(vtable[8]);
                    LOG_INFO("IDXGIFactory::MakeWindowAssociation hooked.");
                }
            }

            // Hook CreateSwapChain (Index 10)
            if (!oIDXGIFactory_CreateSwapChain) {
                if (MH_CreateHook(vtable[10], hkIDXGIFactory_CreateSwapChain, (LPVOID*)&oIDXGIFactory_CreateSwapChain) == MH_OK) {
                    MH_EnableHook(vtable[10]);
                    LOG_INFO("IDXGIFactory::CreateSwapChain hooked.");
                }
            }

            // Try to hook CreateSwapChainForHwnd (Index 15) if IDXGIFactory2
            ComPtr<IDXGIFactory2> pFactory2;
            if (SUCCEEDED(pFactory->QueryInterface(__uuidof(IDXGIFactory2), (void**)pFactory2.ReleaseAndGetAddressOf())) && pFactory2) {
                void** vtable2 = *(void***)pFactory2.Get();
                if (!oIDXGIFactory2_CreateSwapChainForHwnd) {
                    if (MH_CreateHook(vtable2[15], hkIDXGIFactory2_CreateSwapChainForHwnd, (LPVOID*)&oIDXGIFactory2_CreateSwapChainForHwnd) == MH_OK) {
                        MH_EnableHook(vtable2[15]);
                        LOG_INFO("IDXGIFactory2::CreateSwapChainForHwnd hooked.");
                    }
                }
            }
        }

        static HRESULT WINAPI hkCreateDXGIFactory(REFIID riid, void** ppFactory)
        {
            HRESULT hr = oCreateDXGIFactory(riid, ppFactory);
            if (SUCCEEDED(hr) && ppFactory && *ppFactory) {
                InstallFactoryHooks((IUnknown*)*ppFactory);
            }
            return hr;
        }

        static HRESULT WINAPI hkCreateDXGIFactory1(REFIID riid, void** ppFactory)
        {
            HRESULT hr = oCreateDXGIFactory1(riid, ppFactory);
            if (SUCCEEDED(hr) && ppFactory && *ppFactory) {
                InstallFactoryHooks((IUnknown*)*ppFactory);
            }
            return hr;
        }

        static HRESULT WINAPI hkCreateDXGIFactory2(UINT Flags, REFIID riid, void** ppFactory)
        {
            HRESULT hr = oCreateDXGIFactory2(Flags, riid, ppFactory);
            if (SUCCEEDED(hr) && ppFactory && *ppFactory) {
                InstallFactoryHooks((IUnknown*)*ppFactory);
            }
            return hr;
        }

        void InstallD3D9CreateHooks()
        {
            HMODULE hD3D9 = GetModuleHandleA("d3d9.dll");
            if (!hD3D9) return;

            oDirect3DCreate9 = (Direct3DCreate9_t)GetProcAddress(hD3D9, "Direct3DCreate9");
            if (oDirect3DCreate9) {
                if (MH_CreateHook((void*)oDirect3DCreate9, hkDirect3DCreate9, (void**)&oDirect3DCreate9) == MH_OK) {
                    MH_EnableHook((void*)oDirect3DCreate9);
                    LOG_INFO("Direct3DCreate9 hooked successfully.");
                }
            }

            oDirect3DCreate9Ex = (Direct3DCreate9Ex_t)GetProcAddress(hD3D9, "Direct3DCreate9Ex");
            if (oDirect3DCreate9Ex) {
                if (MH_CreateHook((void*)oDirect3DCreate9Ex, hkDirect3DCreate9Ex, (void**)&oDirect3DCreate9Ex) == MH_OK) {
                    MH_EnableHook((void*)oDirect3DCreate9Ex);
                    LOG_INFO("Direct3DCreate9Ex hooked successfully.");
                }
            }
        }

        void InstallDXGICreateHooks()
        {
            HMODULE hDXGI = GetModuleHandleA("dxgi.dll");
            if (!hDXGI) return;

            oCreateDXGIFactory = (CreateDXGIFactory_t)GetProcAddress(hDXGI, "CreateDXGIFactory");
            if (oCreateDXGIFactory) {
                if (MH_CreateHook((void*)oCreateDXGIFactory, hkCreateDXGIFactory, (void**)&oCreateDXGIFactory) == MH_OK) {
                    MH_EnableHook((void*)oCreateDXGIFactory);
                    LOG_INFO("CreateDXGIFactory hooked.");
                }
            }

            oCreateDXGIFactory1 = (CreateDXGIFactory1_t)GetProcAddress(hDXGI, "CreateDXGIFactory1");
            if (oCreateDXGIFactory1) {
                if (MH_CreateHook((void*)oCreateDXGIFactory1, hkCreateDXGIFactory1, (void**)&oCreateDXGIFactory1) == MH_OK) {
                    MH_EnableHook((void*)oCreateDXGIFactory1);
                    LOG_INFO("CreateDXGIFactory1 hooked.");
                }
            }

            oCreateDXGIFactory2 = (CreateDXGIFactory2_t)GetProcAddress(hDXGI, "CreateDXGIFactory2");
            if (oCreateDXGIFactory2) {
                if (MH_CreateHook((void*)oCreateDXGIFactory2, hkCreateDXGIFactory2, (void**)&oCreateDXGIFactory2) == MH_OK) {
                    MH_EnableHook((void*)oCreateDXGIFactory2);
                    LOG_INFO("CreateDXGIFactory2 hooked.");
                }
            }
        }

        void InstallD3D10CreateHooks()
        {
            HMODULE hD3D10 = GetModuleHandleA("d3d10.dll");
            if (!hD3D10) return;

            oD3D10CreateDeviceAndSwapChain = (D3D10CreateDeviceAndSwapChain_t)GetProcAddress(hD3D10, "D3D10CreateDeviceAndSwapChain");
            if (oD3D10CreateDeviceAndSwapChain) {
                if (MH_CreateHook((void*)oD3D10CreateDeviceAndSwapChain, hkD3D10CreateDeviceAndSwapChain, (void**)&oD3D10CreateDeviceAndSwapChain) == MH_OK) {
                    MH_EnableHook((void*)oD3D10CreateDeviceAndSwapChain);
                    LOG_INFO("D3D10CreateDeviceAndSwapChain hooked successfully.");
                }
            }
        }

        void InstallD3D11CreateHooks()
        {
            HMODULE hD3D11 = GetModuleHandleA("d3d11.dll");
            if (!hD3D11) return;

            oD3D11CreateDeviceAndSwapChain = (D3D11CreateDeviceAndSwapChain_t)GetProcAddress(hD3D11, "D3D11CreateDeviceAndSwapChain");
            if (oD3D11CreateDeviceAndSwapChain) {
                if (MH_CreateHook((void*)oD3D11CreateDeviceAndSwapChain, hkD3D11CreateDeviceAndSwapChain, (void**)&oD3D11CreateDeviceAndSwapChain) == MH_OK) {
                    MH_EnableHook((void*)oD3D11CreateDeviceAndSwapChain);
                    LOG_INFO("D3D11CreateDeviceAndSwapChain hooked successfully.");
                }
            }

            oD3D11CreateDevice = (D3D11CreateDevice_t)GetProcAddress(hD3D11, "D3D11CreateDevice");
            if (oD3D11CreateDevice) {
                if (MH_CreateHook((void*)oD3D11CreateDevice, hkD3D11CreateDevice, (void**)&oD3D11CreateDevice) == MH_OK) {
                    MH_EnableHook((void*)oD3D11CreateDevice);
                    LOG_INFO("D3D11CreateDevice hooked successfully.");
                }
            }
        }

        void InstallD3D9CreateHooksLate()
        {
            if (oDirect3DCreate9) {
                IDirect3D9* pDummy = oDirect3DCreate9(D3D_SDK_VERSION);
                if (pDummy) {
                    void** vtable = *(void***)pDummy;
                    if (!oIDirect3D9_CreateDevice) {
                        if (MH_CreateHook(vtable[16], hkIDirect3D9_CreateDevice, (LPVOID*)&oIDirect3D9_CreateDevice) == MH_OK) {
                            MH_EnableHook(vtable[16]);
                            LOG_INFO("IDirect3D9::CreateDevice (VTable) hooked successfully (Late).");
                        }
                    }
                    pDummy->Release();
                }
            }

            if (oDirect3DCreate9Ex) {
                IDirect3D9Ex* pDummyEx = nullptr;
                if (SUCCEEDED(oDirect3DCreate9Ex(D3D_SDK_VERSION, &pDummyEx)) && pDummyEx) {
                    void** vtable = *(void***)pDummyEx;
                    if (!oIDirect3D9_CreateDevice) {
                         if (MH_CreateHook(vtable[16], hkIDirect3D9_CreateDevice, (LPVOID*)&oIDirect3D9_CreateDevice) == MH_OK) {
                             MH_EnableHook(vtable[16]);
                             LOG_INFO("IDirect3D9Ex::CreateDevice (VTable) hooked successfully (Late).");
                         }
                    }
                    if (!oIDirect3D9Ex_CreateDeviceEx) {
                         if (MH_CreateHook(vtable[20], hkIDirect3D9Ex_CreateDeviceEx, (LPVOID*)&oIDirect3D9Ex_CreateDeviceEx) == MH_OK) {
                             MH_EnableHook(vtable[20]);
                             LOG_INFO("IDirect3D9Ex::CreateDeviceEx (VTable) hooked successfully (Late).");
                         }
                    }
                    pDummyEx->Release();
                }
            }
        }
    } // namespace Hooks
} // namespace BaseHook

namespace BaseHook::WindowedMode
{
    static bool s_D3D9Hooked = false;
    static bool s_DXGIHooked = false;
    static bool s_D3D10Hooked = false;
    static bool s_D3D11Hooked = false;

    void InstallD3D9HooksIfNeeded(bool wantD3D9)
    {
        if (!wantD3D9 || s_D3D9Hooked) return;
        Hooks::InstallD3D9CreateHooks();
        s_D3D9Hooked = true;
    }

    void InstallD3D9HooksLate()
    {
        Hooks::InstallD3D9CreateHooksLate();
    }

    void InstallDXGIHooksIfNeeded(bool wantDXGI)
    {
        if (!wantDXGI || s_DXGIHooked) return;
        Hooks::InstallDXGICreateHooks();
        s_DXGIHooked = true;
    }

    void InstallD3D10HooksIfNeeded(bool wantD3D10)
    {
        if (!wantD3D10 || s_D3D10Hooked) return;
        Hooks::InstallD3D10CreateHooks();
        s_D3D10Hooked = true;
    }

    void InstallD3D11HooksIfNeeded(bool wantD3D11)
    {
        if (!wantD3D11 || s_D3D11Hooked) return;
        Hooks::InstallD3D11CreateHooks();
        s_D3D11Hooked = true;
    }
}