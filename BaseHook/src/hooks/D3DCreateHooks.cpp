#include "pch.h"

#include "D3DCreateHooks.h"

#include "base.h"
#include "WindowedMode.h"
#include "log.h"

namespace BaseHook
{
    namespace Hooks
    {
        // --- D3D/DXGI Creation Typedefs ---
        typedef IDirect3D9*(WINAPI* Direct3DCreate9_t)(UINT);
        typedef HRESULT(WINAPI* CreateDXGIFactory_t)(REFIID, void**);
        typedef HRESULT(STDMETHODCALLTYPE* IDirect3D9_CreateDevice_t)(IDirect3D9*, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS*, IDirect3DDevice9**);
        typedef HRESULT(STDMETHODCALLTYPE* IDXGIFactory_CreateSwapChain_t)(IDXGIFactory*, IUnknown*, DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**);
        typedef HRESULT(WINAPI* D3D10CreateDeviceAndSwapChain_t)(IDXGIAdapter*, D3D10_DRIVER_TYPE, HMODULE, UINT, UINT, DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**, ID3D10Device**);
        typedef HRESULT(WINAPI* D3D11CreateDeviceAndSwapChain_t)(IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT, const D3D_FEATURE_LEVEL*, UINT, UINT, const DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**, ID3D11Device**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext**);

        static Direct3DCreate9_t oDirect3DCreate9 = nullptr;
        static CreateDXGIFactory_t oCreateDXGIFactory = nullptr;
        static IDirect3D9_CreateDevice_t oIDirect3D9_CreateDevice = nullptr;
        static IDXGIFactory_CreateSwapChain_t oIDXGIFactory_CreateSwapChain = nullptr;
        static D3D10CreateDeviceAndSwapChain_t oD3D10CreateDeviceAndSwapChain = nullptr;
        static D3D11CreateDeviceAndSwapChain_t oD3D11CreateDeviceAndSwapChain = nullptr;

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
            }

            if (Data::hWindow == NULL && pDesc->OutputWindow != NULL) {
                Data::hWindow = pDesc->OutputWindow;
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

            // Ensure tracking set even if not handling style
            if (pDescToUse && pDescToUse->OutputWindow)
                WindowedMode::SetManagedWindow(pDescToUse->OutputWindow);

            return oD3D10CreateDeviceAndSwapChain(pAdapter, DriverType, Software, Flags, SDKVersion, pDescToUse, ppSwapChain, ppDevice);
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

            // Ensure tracking set even if not handling style
            if (pDescToUse && pDescToUse->OutputWindow)
                WindowedMode::SetManagedWindow(pDescToUse->OutputWindow);

            return oD3D11CreateDeviceAndSwapChain(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion, pDescToUse, ppSwapChain, ppDevice, pFeatureLevel, ppImmediateContext);
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
                pParamsToUse = &params;

                if (Data::hWindow == NULL && hFocusWindow != NULL) {
                    Data::hWindow = hFocusWindow;
                }

                if (Data::hWindow)
                    WindowedMode::SetManagedWindow(Data::hWindow);
            }
            return oIDirect3D9_CreateDevice(pD3D, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pParamsToUse, ppReturnedDeviceInterface);
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

            // Ensure tracking set
            if (SUCCEEDED(hr) && pDescToUse && pDescToUse->OutputWindow)
                WindowedMode::SetManagedWindow(pDescToUse->OutputWindow);

            return hr;
        }

        static HRESULT WINAPI hkCreateDXGIFactory(REFIID riid, void** ppFactory)
        {
            HRESULT hr = oCreateDXGIFactory(riid, ppFactory);
            if (SUCCEEDED(hr) && ppFactory && *ppFactory && !oIDXGIFactory_CreateSwapChain) {
                void** vtable = *(void***)*ppFactory;
                if (MH_CreateHook(vtable[10], hkIDXGIFactory_CreateSwapChain, (LPVOID*)&oIDXGIFactory_CreateSwapChain) == MH_OK)
                    MH_EnableHook(vtable[10]);
            }
            return hr;
        }

        // Exposed installer helpers (called from WindowHooks.cpp).
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
        }

        void InstallDXGICreateHooks()
        {
            HMODULE hDXGI = GetModuleHandleA("dxgi.dll");
            if (!hDXGI) return;

            oCreateDXGIFactory = (CreateDXGIFactory_t)GetProcAddress(hDXGI, "CreateDXGIFactory");
            if (oCreateDXGIFactory) {
                if (MH_CreateHook((void*)oCreateDXGIFactory, hkCreateDXGIFactory, (void**)&oCreateDXGIFactory) == MH_OK) {
                    MH_EnableHook((void*)oCreateDXGIFactory);
                    LOG_INFO("CreateDXGIFactory hooked successfully.");
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


