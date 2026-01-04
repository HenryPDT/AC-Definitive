#include "pch.h"

#include "hooks/D3DCreateHooks.h"
#include "hooks/DXGIHooks.h"
#include "hooks/WindowHooks.h"
#include "hooks/Hooks.h"
#include "hooks/DX9Hooks.h"
#include "hooks/InputHooks.h"
#include "hooks/WindowHooks.h"
#include "hooks/Hooks.h"

#include "core/BaseHook.h"
#include "core/WindowedMode.h"
#include "log.h"
#include "util/ComPtr.h"
#include <d3d9.h>
#include <kiero/minhook/include/MinHook.h>
#include <mutex>

namespace BaseHook
{
    namespace Hooks
    {
        // --- Binding Helpers ---
        static void BindD3D9Hooks(IDirect3DDevice9* pDevice) {
            if (Data::bGraphicsInitialized) return;
            if (!pDevice) return;
            LOG_INFO("BindD3D9Hooks: Capture successful. Device=%p", pDevice);

            void** vtable = *(void***)pDevice;
            
            if (MH_CreateHook(vtable[42], hkEndScene, (LPVOID*)&Data::oEndScene) == MH_OK) {
                MH_EnableHook(vtable[42]);
                LOG_INFO("D3D9 Hook: EndScene hooked.");
            }
            if (MH_CreateHook(vtable[16], hkReset, (LPVOID*)&Data::oReset) == MH_OK) {
                MH_EnableHook(vtable[16]);
                LOG_INFO("D3D9 Hook: Reset hooked.");
            }
            if (MH_CreateHook(vtable[17], hkPresent9, (LPVOID*)&Data::oPresent9) == MH_OK) {
                MH_EnableHook(vtable[17]);
                LOG_INFO("D3D9 Hook: Present hooked.");
            }
            if (MH_CreateHook(vtable[3], hkTestCooperativeLevel, (LPVOID*)&Data::oTestCooperativeLevel) == MH_OK) {
                MH_EnableHook(vtable[3]);
                LOG_INFO("D3D9 Hook: TestCooperativeLevel hooked.");
            }

            Data::bGraphicsInitialized = true;
            Hooks::FinishInitialization();
        }

        static void BindD3D11Hooks(IDXGISwapChain* pSwapChain) {
            if (Data::bGraphicsInitialized) return;
            if (!pSwapChain) return;
            LOG_INFO("BindD3D11Hooks: Capture successful. SwapChain=%p", pSwapChain);

            void** vtable = *(void***)pSwapChain;
            
            if (MH_CreateHook(vtable[8], hkPresentDX11, (LPVOID*)&Data::oPresent) == MH_OK) {
                MH_EnableHook(vtable[8]);
                LOG_INFO("DX11 Hook: Present hooked.");
            }
            if (MH_CreateHook(vtable[13], hkResizeBuffersDX11, (LPVOID*)&Data::oResizeBuffers) == MH_OK) {
                MH_EnableHook(vtable[13]);
                LOG_INFO("DX11 Hook: ResizeBuffers hooked.");
            }
            if (MH_CreateHook(vtable[14], hkResizeTargetDX11, (LPVOID*)&Data::oResizeTarget) == MH_OK) {
                MH_EnableHook(vtable[14]);
                LOG_INFO("DX11 Hook: ResizeTarget hooked.");
            }

            Data::bGraphicsInitialized = true;
            Hooks::FinishInitialization();
        }

        static void BindD3D10Hooks(IDXGISwapChain* pSwapChain) {
            if (Data::bGraphicsInitialized) return;
            if (!pSwapChain) return;
            LOG_INFO("BindD3D10Hooks: Capture successful. SwapChain=%p", pSwapChain);

            void** vtable = *(void***)pSwapChain;
            
            if (MH_CreateHook(vtable[8], hkPresentDX10, (LPVOID*)&Data::oPresent) == MH_OK) {
                MH_EnableHook(vtable[8]);
                LOG_INFO("DX10 Hook: Present hooked.");
            }
            if (MH_CreateHook(vtable[13], hkResizeBuffersDX10, (LPVOID*)&Data::oResizeBuffers) == MH_OK) {
                MH_EnableHook(vtable[13]);
                LOG_INFO("DX10 Hook: ResizeBuffers hooked.");
            }
            if (MH_CreateHook(vtable[14], hkResizeTargetDX10, (LPVOID*)&Data::oResizeTarget) == MH_OK) {
                MH_EnableHook(vtable[14]);
                LOG_INFO("DX10 Hook: ResizeTarget hooked.");
            }

            Data::bGraphicsInitialized = true;
            Hooks::FinishInitialization();
        }

        static void BindGenericDXGIHooks(IDXGISwapChain* pSwapChain, IUnknown* pDevice) {
            if (Data::bGraphicsInitialized) return;
            if (!pSwapChain || !pDevice) return;

            ComPtr<ID3D11Device> d3d11;
            if (SUCCEEDED(pDevice->QueryInterface(__uuidof(ID3D11Device), (void**)d3d11.GetAddressOf()))) {
                BindD3D11Hooks(pSwapChain);
                return;
            }

            ComPtr<ID3D10Device> d3d10;
            if (SUCCEEDED(pDevice->QueryInterface(__uuidof(ID3D10Device), (void**)d3d10.GetAddressOf()))) {
                BindD3D10Hooks(pSwapChain);
                return;
            }
            
            LOG_WARN("BindGenericDXGIHooks: Could not determine device type.");
        }

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
            }

            HRESULT hr = oD3D10CreateDeviceAndSwapChain(pAdapter, DriverType, Software, Flags, SDKVersion, pDescToUse, ppSwapChain, ppDevice);

            if (SUCCEEDED(hr) && ppSwapChain && *ppSwapChain)
            {
                BindD3D10Hooks(*ppSwapChain);

                DXGI_SWAP_CHAIN_DESC desc;
                if (SUCCEEDED((*ppSwapChain)->GetDesc(&desc)))
                {
                    if (desc.BufferDesc.Width > 0 && desc.BufferDesc.Height > 0)
                        WindowedMode::NotifyResolutionChange(desc.BufferDesc.Width, desc.BufferDesc.Height);
                }

                EnsureSwapChainFullscreenHook(*ppSwapChain);
                DisableDXGIAltEnter(*ppSwapChain);

                // Apply window settings after successful creation to avoid interference during creation
                if (WindowedMode::ShouldHandle() && Data::hWindow) {
                    WindowedMode::Apply(Data::hWindow);
                }
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
            }

            HRESULT hr = oD3D11CreateDeviceAndSwapChain(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion, pDescToUse, ppSwapChain, ppDevice, pFeatureLevel, ppImmediateContext);

            if (SUCCEEDED(hr) && ppSwapChain && *ppSwapChain)
            {
                BindD3D11Hooks(*ppSwapChain);

                DXGI_SWAP_CHAIN_DESC desc;
                if (SUCCEEDED((*ppSwapChain)->GetDesc(&desc)))
                {
                    if (desc.BufferDesc.Width > 0 && desc.BufferDesc.Height > 0)
                        WindowedMode::NotifyResolutionChange(desc.BufferDesc.Width, desc.BufferDesc.Height);
                }

                EnsureSwapChainFullscreenHook(*ppSwapChain);
                DisableDXGIAltEnter(*ppSwapChain);

                if (WindowedMode::ShouldHandle() && Data::hWindow) {
                    WindowedMode::Apply(Data::hWindow);
                }
            }

            return hr;
        }

        static HRESULT STDMETHODCALLTYPE hkIDirect3D9_CreateDevice(IDirect3D9* pD3D, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice9** ppReturnedDeviceInterface)
        {
            WindowedMode::CheckAndApplyPendingState();

            D3DPRESENT_PARAMETERS params;
            if (pPresentationParameters) params = *pPresentationParameters;
            D3DPRESENT_PARAMETERS* pParamsToUse = pPresentationParameters;

            if (pPresentationParameters) {
                if ((WindowedMode::ShouldHandle() || WindowedMode::ShouldApplyResolutionOverride()) && WindowedMode::g_State.overrideWidth > 0 && WindowedMode::g_State.overrideHeight > 0)
                {
                    params.BackBufferWidth = WindowedMode::g_State.overrideWidth;
                    params.BackBufferHeight = WindowedMode::g_State.overrideHeight;
                }

                if (params.BackBufferWidth > 0 && params.BackBufferHeight > 0) {
                    WindowedMode::NotifyResolutionChange(params.BackBufferWidth, params.BackBufferHeight);
                }

                if (WindowedMode::ShouldHandle()) {
                    LOG_INFO("hkIDirect3D9_CreateDevice: Forced windowed.");
                    params.Windowed = TRUE;
                    params.FullScreen_RefreshRateInHz = 0;
                }
                else if (WindowedMode::g_State.activeMode == WindowedMode::Mode::ExclusiveFullscreen) {
                    LOG_INFO("hkIDirect3D9_CreateDevice: Forced exclusive fullscreen.");
                    params.Windowed = FALSE;
                }
                
                // --- FIX FOR PROXY WINDOWS (AC1) ---
                if (params.hDeviceWindow != NULL && hFocusWindow != NULL && params.hDeviceWindow != hFocusWindow)
                {
                    if (WindowedMode::ShouldHandle())
                    {
                        LOG_INFO("hkIDirect3D9_CreateDevice: Overriding hDeviceWindow (%p -> %p) to prevent proxy window usage.", params.hDeviceWindow, hFocusWindow);
                        params.hDeviceWindow = hFocusWindow;
                    }
                }
                else if (params.hDeviceWindow != NULL && Data::hWindow != NULL && params.hDeviceWindow != Data::hWindow)
                {
                     if (WindowedMode::ShouldHandle())
                    {
                        LOG_INFO("hkIDirect3D9_CreateDevice: Overriding hDeviceWindow (%p -> %p) to prevent proxy window usage.", params.hDeviceWindow, Data::hWindow);
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
                }
            }

            HRESULT hr = oIDirect3D9_CreateDevice(pD3D, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pParamsToUse, ppReturnedDeviceInterface);
            if (SUCCEEDED(hr) && ppReturnedDeviceInterface && *ppReturnedDeviceInterface)
            {
                // Write back the actual parameters used (including Windowed=TRUE) to the game's struct.
                // This prevents the game from thinking it's in Fullscreen when the device is actually Windowed.
                if (pPresentationParameters && pParamsToUse == &params) {
                    *pPresentationParameters = params;
                }

                BindD3D9Hooks(*ppReturnedDeviceInterface);

                IDirect3DSurface9* pBackBuffer = nullptr;
                if (SUCCEEDED((*ppReturnedDeviceInterface)->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer))) {
                    D3DSURFACE_DESC desc;
                    pBackBuffer->GetDesc(&desc);
                    pBackBuffer->Release();
                    WindowedMode::NotifyResolutionChange(desc.Width, desc.Height);
                }

                if (WindowedMode::ShouldHandle() && Data::hWindow) {
                    WindowedMode::Apply(Data::hWindow);
                }

            }

            return hr;
        }

        static HRESULT STDMETHODCALLTYPE hkIDirect3D9Ex_CreateDeviceEx(IDirect3D9Ex* pD3D, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, D3DDISPLAYMODEEX* pFullscreenDisplayMode, IDirect3DDevice9Ex** ppReturnedDeviceInterface)
        {
            WindowedMode::CheckAndApplyPendingState();

            D3DPRESENT_PARAMETERS params;
            if (pPresentationParameters) params = *pPresentationParameters;
            D3DPRESENT_PARAMETERS* pParamsToUse = pPresentationParameters;
            D3DDISPLAYMODEEX* pFullscreenModeToUse = pFullscreenDisplayMode;

            if (pPresentationParameters) {
                if ((WindowedMode::ShouldHandle() || WindowedMode::ShouldApplyResolutionOverride()) && WindowedMode::g_State.overrideWidth > 0 && WindowedMode::g_State.overrideHeight > 0)
                {
                    params.BackBufferWidth = WindowedMode::g_State.overrideWidth;
                    params.BackBufferHeight = WindowedMode::g_State.overrideHeight;
                }

                if (params.BackBufferWidth > 0 && params.BackBufferHeight > 0) {
                    WindowedMode::NotifyResolutionChange(params.BackBufferWidth, params.BackBufferHeight);
                }

                if (WindowedMode::ShouldHandle()) {
                    LOG_INFO("hkIDirect3D9Ex_CreateDeviceEx: Forced windowed.");
                    params.Windowed = TRUE;
                    params.FullScreen_RefreshRateInHz = 0;
                    pFullscreenModeToUse = nullptr;
                }
                else if (WindowedMode::g_State.activeMode == WindowedMode::Mode::ExclusiveFullscreen) {
                    LOG_INFO("hkIDirect3D9Ex_CreateDeviceEx: Forced exclusive fullscreen.");
                    params.Windowed = FALSE;
                }
                
                // --- FIX FOR PROXY WINDOWS (AC1) ---
                if (params.hDeviceWindow != NULL && hFocusWindow != NULL && params.hDeviceWindow != hFocusWindow)
                {
                    if (WindowedMode::ShouldHandle())
                    {
                        LOG_INFO("hkIDirect3D9Ex_CreateDeviceEx: Overriding hDeviceWindow (%p -> %p) to prevent proxy window usage.", params.hDeviceWindow, hFocusWindow);
                        params.hDeviceWindow = hFocusWindow;
                    }
                }
                else if (params.hDeviceWindow != NULL && Data::hWindow != NULL && params.hDeviceWindow != Data::hWindow)
                {
                     if (WindowedMode::ShouldHandle())
                    {
                        LOG_INFO("hkIDirect3D9Ex_CreateDeviceEx: Overriding hDeviceWindow (%p -> %p) to prevent proxy window usage.", params.hDeviceWindow, Data::hWindow);
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
                }
            }

            HRESULT hr = oIDirect3D9Ex_CreateDeviceEx(pD3D, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pParamsToUse, pFullscreenModeToUse, ppReturnedDeviceInterface);

            if (SUCCEEDED(hr) && ppReturnedDeviceInterface && *ppReturnedDeviceInterface)
            {
                if (pPresentationParameters && pParamsToUse == &params) {
                    *pPresentationParameters = params;
                }

                BindD3D9Hooks(*ppReturnedDeviceInterface);

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

                if (WindowedMode::ShouldHandle() && Data::hWindow) {
                    WindowedMode::Apply(Data::hWindow);
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
                {
                    MH_EnableHook(vtable[16]);
                    LOG_INFO("hkDirect3DCreate9: Hooked IDirect3D9::CreateDevice.");
                }
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
                {
                    MH_EnableHook(vtable[16]);
                    LOG_INFO("hkDirect3DCreate9Ex: Hooked IDirect3D9::CreateDevice.");
                }
                if (!oIDirect3D9Ex_CreateDeviceEx && MH_CreateHook(vtable[20], hkIDirect3D9Ex_CreateDeviceEx, (LPVOID*)&oIDirect3D9Ex_CreateDeviceEx) == MH_OK)
                {
                    MH_EnableHook(vtable[20]);
                    LOG_INFO("hkDirect3DCreate9Ex: Hooked IDirect3D9Ex::CreateDeviceEx.");
                }
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
                BindGenericDXGIHooks(*ppSwapChain, pDevice);

                DXGI_SWAP_CHAIN_DESC desc;
                if (SUCCEEDED((*ppSwapChain)->GetDesc(&desc)))
                {
                    if (desc.BufferDesc.Width > 0 && desc.BufferDesc.Height > 0)
                        WindowedMode::NotifyResolutionChange(desc.BufferDesc.Width, desc.BufferDesc.Height);
                }

                if (pDescToUse && pDescToUse->OutputWindow)
                    WindowedMode::SetManagedWindow(pDescToUse->OutputWindow);

                EnsureSwapChainFullscreenHook(*ppSwapChain);
                DisableDXGIAltEnter(*ppSwapChain);

                if (WindowedMode::ShouldHandle() && Data::hWindow) {
                    WindowedMode::Apply(Data::hWindow);
                }
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
                if (hWnd)
                    WindowedMode::SetManagedWindow(hWnd);
            }

            HRESULT hr = oIDXGIFactory2_CreateSwapChainForHwnd(pFactory, pDevice, hWnd, pDescToUse, pFSDescToUse, pRestrictToOutput, ppSwapChain);

            if (SUCCEEDED(hr) && ppSwapChain && *ppSwapChain)
            {
                BindGenericDXGIHooks(*ppSwapChain, pDevice);

                DXGI_SWAP_CHAIN_DESC desc;
                if (SUCCEEDED((*ppSwapChain)->GetDesc(&desc)))
                {
                    if (desc.BufferDesc.Width > 0 && desc.BufferDesc.Height > 0)
                        WindowedMode::NotifyResolutionChange(desc.BufferDesc.Width, desc.BufferDesc.Height);
                }

                EnsureSwapChainFullscreenHook(*ppSwapChain);
                DisableDXGIAltEnter(*ppSwapChain);

                if (WindowedMode::ShouldHandle() && Data::hWindow) {
                    WindowedMode::Apply(Data::hWindow);
                }
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
            static std::recursive_mutex s_Mutex;
            static bool s_LateHooksInstalled = false;
            static bool s_IsInstalling = false;

            std::lock_guard<std::recursive_mutex> lock(s_Mutex);
            if (s_LateHooksInstalled || s_IsInstalling) return;

            s_IsInstalling = true;

            if (oDirect3DCreate9) {
                ComPtr<IDirect3D9> pDummy;
                pDummy.Attach(oDirect3DCreate9(D3D_SDK_VERSION));
                
                if (pDummy) {
                    void** vtable = *(void***)pDummy.Get();
                    if (!oIDirect3D9_CreateDevice) {
                        if (MH_CreateHook(vtable[16], hkIDirect3D9_CreateDevice, (LPVOID*)&oIDirect3D9_CreateDevice) == MH_OK) {
                            MH_EnableHook(vtable[16]);
                            LOG_INFO("IDirect3D9::CreateDevice (VTable) hooked successfully (Late).");
                        }
                    }
                }
            }

            if (oDirect3DCreate9Ex) {
                ComPtr<IDirect3D9Ex> pDummyEx;
                if (SUCCEEDED(oDirect3DCreate9Ex(D3D_SDK_VERSION, pDummyEx.ReleaseAndGetAddressOf())) && pDummyEx) {
                    void** vtable = *(void***)pDummyEx.Get();
                    if (!oIDirect3D9_CreateDevice) {
                         if (MH_CreateHook(vtable[16], hkIDirect3D9_CreateDevice, (LPVOID*)&oIDirect3D9_CreateDevice) == MH_OK) {
                             MH_EnableHook(vtable[16]);
                             LOG_INFO("IDirect3D9::CreateDevice (VTable) hooked successfully (Late).");
                         }
                    }
                    if (!oIDirect3D9Ex_CreateDeviceEx) {
                         if (MH_CreateHook(vtable[20], hkIDirect3D9Ex_CreateDeviceEx, (LPVOID*)&oIDirect3D9Ex_CreateDeviceEx) == MH_OK) {
                             MH_EnableHook(vtable[20]);
                             LOG_INFO("IDirect3D9Ex::CreateDeviceEx (VTable) hooked successfully (Late).");
                         }
                    }
                }
            }

            s_LateHooksInstalled = true;
            s_IsInstalling = false;
        }

        // --- Generic DXGI Hooks for Late Injection ---
        // These handle dispatching to DX10 or DX11 based on device type

        static HRESULT __stdcall hkPresentDXGIGeneric(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
        {
            if (Data::bIsDetached) return Data::oPresent(pSwapChain, SyncInterval, Flags);

            // Use configuration hint if available to avoid costly QueryInterface or if detection fails
            if (WindowedMode::g_State.targetDXVersion == 11) return hkPresentDX11(pSwapChain, SyncInterval, Flags);
            if (WindowedMode::g_State.targetDXVersion == 10) return hkPresentDX10(pSwapChain, SyncInterval, Flags);

            if (!Data::bIsInitialized) {
                ComPtr<ID3D11Device> dev11;
                if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)dev11.GetAddressOf()))) {
                    return hkPresentDX11(pSwapChain, SyncInterval, Flags);
                }
                ComPtr<ID3D10Device> dev10;
                if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D10Device), (void**)dev10.GetAddressOf()))) {
                    return hkPresentDX10(pSwapChain, SyncInterval, Flags);
                }
                // Fallback for D3D10.1 (Some DX10 games use 10.1 interfaces)
                ComPtr<ID3D10Device1> dev10_1;
                if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D10Device1), (void**)dev10_1.GetAddressOf()))) {
                    return hkPresentDX10(pSwapChain, SyncInterval, Flags);
                }
            }
            else {
                if (Data::pDevice11) return hkPresentDX11(pSwapChain, SyncInterval, Flags);
                if (Data::pDevice10) return hkPresentDX10(pSwapChain, SyncInterval, Flags);
            }

            // Only warn once if we fail to route
            static bool s_Warned = false;
            if (!s_Warned) { LOG_WARN("hkPresentDXGIGeneric: Failed to detect D3D10/11 device."); s_Warned = true; }

            return Data::oPresent(pSwapChain, SyncInterval, Flags);
        }

        static HRESULT __stdcall hkResizeBuffersDXGIGeneric(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
        {
            if (Data::bIsDetached) return Data::oResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);

            if (WindowedMode::g_State.targetDXVersion == 11) return hkResizeBuffersDX11(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
            if (WindowedMode::g_State.targetDXVersion == 10) return hkResizeBuffersDX10(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);

            if (Data::pDevice11) return hkResizeBuffersDX11(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
            if (Data::pDevice10) return hkResizeBuffersDX10(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);

            // If not initialized yet, try to detect
            ComPtr<ID3D11Device> dev11;
            if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)dev11.GetAddressOf()))) {
                return hkResizeBuffersDX11(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
            }
            return hkResizeBuffersDX10(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
        }

        static HRESULT __stdcall hkResizeTargetDXGIGeneric(IDXGISwapChain* pSwapChain, const DXGI_MODE_DESC* pNewTargetParameters)
        {
            if (Data::bIsDetached) return Data::oResizeTarget(pSwapChain, pNewTargetParameters);
            
            if (WindowedMode::g_State.targetDXVersion == 11) return hkResizeTargetDX11(pSwapChain, pNewTargetParameters);
            if (WindowedMode::g_State.targetDXVersion == 10) return hkResizeTargetDX10(pSwapChain, pNewTargetParameters);

            if (Data::pDevice11) return hkResizeTargetDX11(pSwapChain, pNewTargetParameters);
            if (Data::pDevice10) return hkResizeTargetDX10(pSwapChain, pNewTargetParameters);
            
            ComPtr<ID3D11Device> dev11;
            if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)dev11.GetAddressOf()))) {
                return hkResizeTargetDX11(pSwapChain, pNewTargetParameters);
            }
            return hkResizeTargetDX10(pSwapChain, pNewTargetParameters);
        }

        void InstallDXGIHooksLate()
        {
            static std::recursive_mutex s_Mutex;
            static bool s_DXGILateHooksInstalled = false;
            static bool s_IsInstalling = false;

            std::lock_guard<std::recursive_mutex> lock(s_Mutex);
            if (s_DXGILateHooksInstalled || s_IsInstalling) return;

            s_IsInstalling = true;

            // If hooks are already bound via creation hook, skip
            if (Data::oPresent) {
                s_DXGILateHooksInstalled = true;
                s_IsInstalling = false;
                return;
            }

            LOG_INFO("InstallDXGIHooksLate: Starting...");
            
            WNDCLASSEX wc = { 0 };
            wc.cbSize = sizeof(wc);
            wc.lpfnWndProc = DefWindowProc;
            wc.lpszClassName = TEXT("DummyWindow");
            RegisterClassEx(&wc);
            HWND hWnd = CreateWindow(wc.lpszClassName, TEXT("Dummy"), 0, 0, 0, 100, 100, NULL, NULL, NULL, NULL);

            if (!hWnd) {
                LOG_ERROR("InstallDXGIHooksLate: Failed to create dummy window.");
                s_IsInstalling = false;
                return;
            }

            int targetDX = WindowedMode::g_State.targetDXVersion;
            HMODULE hD3D11 = (targetDX == 0 || targetDX == 11) ? GetModuleHandleA("d3d11.dll") : NULL;
            HMODULE hD3D10 = (targetDX == 0 || targetDX == 10) ? GetModuleHandleA("d3d10.dll") : NULL;
            
            ComPtr<IDXGISwapChain> pSwapChain;
            ComPtr<ID3D11Device> pDev11;
            ComPtr<ID3D10Device> pDev10;
            HRESULT hr = E_FAIL;

            if (hD3D11) {
                D3D11CreateDeviceAndSwapChain_t createFn = (D3D11CreateDeviceAndSwapChain_t)GetProcAddress(hD3D11, "D3D11CreateDeviceAndSwapChain");
                if (createFn) {
                    DXGI_SWAP_CHAIN_DESC desc = { 0 };
                    desc.BufferCount = 1;
                    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
                    desc.OutputWindow = hWnd;
                    desc.SampleDesc.Count = 1;
                    desc.Windowed = TRUE;
                    
                    D3D_FEATURE_LEVEL feats[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
                    hr = createFn(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, feats, 2, D3D11_SDK_VERSION, &desc, pSwapChain.ReleaseAndGetAddressOf(), pDev11.ReleaseAndGetAddressOf(), NULL, NULL);
                    if (SUCCEEDED(hr)) {
                        LOG_INFO("InstallDXGIHooksLate: Dummy SwapChain created via D3D11.");
                    }
                }
            }

            if (FAILED(hr) && hD3D10) {
                D3D10CreateDeviceAndSwapChain_t createFn = (D3D10CreateDeviceAndSwapChain_t)GetProcAddress(hD3D10, "D3D10CreateDeviceAndSwapChain");
                if (createFn) {
                    DXGI_SWAP_CHAIN_DESC desc = { 0 };
                    desc.BufferCount = 1;
                    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
                    desc.OutputWindow = hWnd;
                    desc.SampleDesc.Count = 1;
                    desc.Windowed = TRUE;
                    hr = createFn(NULL, D3D10_DRIVER_TYPE_HARDWARE, NULL, 0, D3D10_SDK_VERSION, &desc, pSwapChain.ReleaseAndGetAddressOf(), pDev10.ReleaseAndGetAddressOf());
                    if (SUCCEEDED(hr)) {
                        LOG_INFO("InstallDXGIHooksLate: Dummy SwapChain created via D3D10.");
                    }
                }
            }

            if (SUCCEEDED(hr) && pSwapChain) {
                void** vtable = *(void***)pSwapChain.Get();
                MH_CreateHook(vtable[8], hkPresentDXGIGeneric, (LPVOID*)&Data::oPresent);
                MH_EnableHook(vtable[8]);
                MH_CreateHook(vtable[13], hkResizeBuffersDXGIGeneric, (LPVOID*)&Data::oResizeBuffers);
                MH_EnableHook(vtable[13]);
                MH_CreateHook(vtable[14], hkResizeTargetDXGIGeneric, (LPVOID*)&Data::oResizeTarget);
                MH_EnableHook(vtable[14]);
                
                LOG_INFO("InstallDXGIHooksLate: Hooks installed via dummy swapchain.");
            } else {
                LOG_ERROR("InstallDXGIHooksLate: Failed to create dummy device/swapchain.");
            }
            
            DestroyWindow(hWnd);
            UnregisterClass(wc.lpszClassName, NULL);
            
            s_DXGILateHooksInstalled = true;
            s_IsInstalling = false;
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

    void InstallDXGIHooksLate()
    {
        Hooks::InstallDXGIHooksLate();
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