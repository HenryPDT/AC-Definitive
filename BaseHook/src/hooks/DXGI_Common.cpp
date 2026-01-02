#include "pch.h"
#include "hooks/DXGI_Common.h"

#include "WindowedMode.h"
#include "log.h"
#include "FramerateLimiter.h"
#include "ComPtr.h"

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx10.h"
#include "imgui_impl_dx11.h"

namespace BaseHook::Hooks::DXGICommon
{
    typedef HRESULT(STDMETHODCALLTYPE* IDXGISwapChain_SetFullscreenState_t)(IDXGISwapChain*, BOOL, IDXGIOutput*);
    static IDXGISwapChain_SetFullscreenState_t s_oSetFullscreenState = nullptr;

    typedef HRESULT(STDMETHODCALLTYPE* IDXGISwapChain_GetFullscreenState_t)(IDXGISwapChain*, BOOL*, IDXGIOutput**);
    static IDXGISwapChain_GetFullscreenState_t s_oGetFullscreenState = nullptr;

    typedef HRESULT(STDMETHODCALLTYPE* IDXGISwapChain_GetDesc_t)(IDXGISwapChain*, DXGI_SWAP_CHAIN_DESC*);
    static IDXGISwapChain_GetDesc_t s_oGetDesc = nullptr;

    static HRESULT STDMETHODCALLTYPE hkIDXGISwapChain_GetFullscreenState_DXGICommon(IDXGISwapChain* pSwapChain, BOOL* pFullscreen, IDXGIOutput** ppTarget)
    {
        HRESULT hr = s_oGetFullscreenState ? s_oGetFullscreenState(pSwapChain, pFullscreen, ppTarget) : S_OK;

        if (SUCCEEDED(hr) && pFullscreen && WindowedMode::ShouldHandle())
        {
            // "Fake Fullscreen" Logic:
            // If we are in Borderless Fullscreen mode, the game likely expects to be in "Exclusive Fullscreen".
            // Since we blocked the transition to Exclusive (SetFullscreenState), we lie here and say we ARE in Fullscreen.
            if (WindowedMode::g_State.activeMode == WindowedMode::Mode::BorderlessFullscreen) {
                *pFullscreen = TRUE;
            }
            else {
                // For standard Bordered Windowed, we tell the truth (FALSE).
                *pFullscreen = FALSE;
            }
        }
        return hr;
    }

    static HRESULT STDMETHODCALLTYPE hkIDXGISwapChain_GetDesc_DXGICommon(IDXGISwapChain* pSwapChain, DXGI_SWAP_CHAIN_DESC* pDesc)
    {
        HRESULT hr = s_oGetDesc ? s_oGetDesc(pSwapChain, pDesc) : S_OK;

        if (SUCCEEDED(hr) && pDesc && WindowedMode::ShouldHandle())
        {
            // Consistent Lie:
            // If we told the game we are Fullscreen (in GetFullscreenState), we must also report Windowed = FALSE here.
            if (WindowedMode::g_State.activeMode == WindowedMode::Mode::BorderlessFullscreen) {
                pDesc->Windowed = FALSE;
            }
            // Note: We do NOT overwrite BufferDesc dimensions here because the game might use them to resize its backbuffers.
            // We want the backbuffers to match our virtual resolution.
        }
        return hr;
    }

    static HRESULT STDMETHODCALLTYPE hkIDXGISwapChain_SetFullscreenState_DXGICommon(IDXGISwapChain* pSwapChain, BOOL Fullscreen, IDXGIOutput* pTarget)
    {
        const bool altDown = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
        const bool enterDown = (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0;

        // If user selected a windowed mode, block attempts to go fullscreen.
        if (WindowedMode::ShouldHandle() && Fullscreen)
        {
            static ULONGLONG s_lastLog = 0;
            const ULONGLONG now = GetTickCount64();
            if (now - s_lastLog > 5000)
            {
                s_lastLog = now;
                LOG_INFO("DXGI(SetFullscreenState): Blocked TRUE -> forcing FALSE (windowed mode configured).");
            }
            return s_oSetFullscreenState ? s_oSetFullscreenState(pSwapChain, FALSE, nullptr) : S_OK;
        }

        // If user selected exclusive fullscreen, block Alt+Enter attempts to go windowed.
        if (!WindowedMode::ShouldHandle() && !Fullscreen && (altDown && enterDown))
        {
            static ULONGLONG s_lastLog = 0;
            const ULONGLONG now = GetTickCount64();
            if (now - s_lastLog > 1000)
            {
                s_lastLog = now;
                LOG_INFO("DXGI(SetFullscreenState): Blocked FALSE due to Alt+Enter (exclusive fullscreen configured).");
            }
            return S_OK;
        }

        return s_oSetFullscreenState ? s_oSetFullscreenState(pSwapChain, Fullscreen, pTarget) : S_OK;
    }

    void EnsureSwapChainFullscreenHook(IDXGISwapChain* pSwapChain)
    {
        static IDXGISwapChain* s_hookedSwapChain = nullptr;
        if (!pSwapChain || s_hookedSwapChain == pSwapChain)
            return;

        void** vtable = *(void***)pSwapChain;
        if (!vtable)
            return;

        bool hookedAny = false;

        // vtable[10] = IDXGISwapChain::SetFullscreenState
        if (MH_CreateHook(vtable[10], hkIDXGISwapChain_SetFullscreenState_DXGICommon, (LPVOID*)&s_oSetFullscreenState) == MH_OK)
        {
            MH_EnableHook(vtable[10]);
            hookedAny = true;
        }

        // vtable[11] = IDXGISwapChain::GetFullscreenState
        if (MH_CreateHook(vtable[11], hkIDXGISwapChain_GetFullscreenState_DXGICommon, (LPVOID*)&s_oGetFullscreenState) == MH_OK)
        {
            MH_EnableHook(vtable[11]);
            hookedAny = true;
        }

        // vtable[12] = IDXGISwapChain::GetDesc
        if (MH_CreateHook(vtable[12], hkIDXGISwapChain_GetDesc_DXGICommon, (LPVOID*)&s_oGetDesc) == MH_OK)
        {
            MH_EnableHook(vtable[12]);
            hookedAny = true;
        }

        if (hookedAny)
        {
            s_hookedSwapChain = pSwapChain;
        }
    }

    void DisableDXGIAltEnter(IDXGISwapChain* pSwapChain)
    {
        static IDXGISwapChain* s_lastSwapChain = nullptr;
        static bool s_done = false;

        if (!pSwapChain)
            return;

        // Only need to do this once per swapchain.
        if (s_done && s_lastSwapChain == pSwapChain)
            return;

        s_lastSwapChain = pSwapChain;
        s_done = true;

        ComPtr<IDXGIFactory> factory;
        HRESULT hr = pSwapChain->GetParent(IID_PPV_ARGS(factory.ReleaseAndGetAddressOf()));
        if (FAILED(hr) || !factory)
        {
            return;
        }

        if (!Data::hWindow || !IsWindow(Data::hWindow))
            return;

        // Disable DXGI runtime Alt+Enter fullscreen toggling.
        factory->MakeWindowAssociation(Data::hWindow, DXGI_MWA_NO_ALT_ENTER);
    }

    static void CreateRenderTarget10(IDXGISwapChain* pSwapChain)
    {
        ComPtr<ID3D10Texture2D> pBackBuffer;
        pSwapChain->GetBuffer(0, IID_PPV_ARGS(pBackBuffer.ReleaseAndGetAddressOf()));
        if (pBackBuffer)
        {
            Data::pDevice10->CreateRenderTargetView(pBackBuffer.Get(), NULL, &Data::pMainRenderTargetView10);
        }
    }

    static void CleanupRenderTarget10()
    {
        if (Data::pMainRenderTargetView10) { Data::pMainRenderTargetView10->Release(); Data::pMainRenderTargetView10 = NULL; }
    }

    static void CreateRenderTarget11(IDXGISwapChain* pSwapChain)
    {
        ComPtr<ID3D11Texture2D> pBackBuffer;
        pSwapChain->GetBuffer(0, IID_PPV_ARGS(pBackBuffer.ReleaseAndGetAddressOf()));
        if (pBackBuffer)
        {
            Data::pDevice11->CreateRenderTargetView(pBackBuffer.Get(), NULL, &Data::pMainRenderTargetView11);
        }
    }

    static void CleanupRenderTarget11()
    {
        if (Data::pMainRenderTargetView11) { Data::pMainRenderTargetView11->Release(); Data::pMainRenderTargetView11 = NULL; }
    }

    static void EnsureWndProcForSwapChain(IDXGISwapChain* pSwapChain)
    {
        DXGI_SWAP_CHAIN_DESC desc;
        pSwapChain->GetDesc(&desc);
        if (desc.OutputWindow && desc.OutputWindow != Data::hWindow)
        {
            LOG_INFO("Window Changed (DXGI). Re-hooking WndProc.");
            RestoreWndProc();
            Data::hWindow = desc.OutputWindow;
            InstallWndProcHook();
        }
    }

    bool EnsureInitialized(Api api, IDXGISwapChain* pSwapChain)
    {
        if (Data::bIsInitialized)
            return true;

        Data::pSwapChain = pSwapChain;
        EnsureSwapChainFullscreenHook(pSwapChain);

        if (WindowedMode::ShouldHandle())
        {
            pSwapChain->SetFullscreenState(FALSE, nullptr);
            // Re-apply window settings immediately to fix styles/rects
            WindowedMode::Apply(Data::hWindow);
        }

        if (api == Api::D3D10)
        {
            if (FAILED(pSwapChain->GetDevice(__uuidof(ID3D10Device), (void**)&Data::pDevice10)))
            {
                // Fallback for D3D10.1
                if (FAILED(pSwapChain->GetDevice(__uuidof(ID3D10Device1), (void**)&Data::pDevice10)))
                {
                    static bool s_logDevFail = false;
                    if (!s_logDevFail) { LOG_ERROR("DXGI EnsureInitialized (D3D10): Failed to get ID3D10Device/1 from swapchain."); s_logDevFail = true; }
                    return false;
                }
                LOG_INFO("DXGI EnsureInitialized: Retrieved ID3D10Device (via 10.1/10 query).");
            }

            EnsureWndProcForSwapChain(pSwapChain);
            DisableDXGIAltEnter(pSwapChain);

            InitImGuiStyle();
            ImGui_ImplWin32_Init(Data::hWindow);
            ImGui_ImplDX10_Init(Data::pDevice10);
            CreateRenderTarget10(pSwapChain);
            Data::bIsInitialized = true;
            return true;
        }

        // D3D11
        if (FAILED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&Data::pDevice11)))
        {
            static bool s_logDevFail = false;
            if (!s_logDevFail) { LOG_ERROR("DXGI EnsureInitialized (D3D11): Failed to get ID3D11Device from swapchain."); s_logDevFail = true; }
            return false;
        }

        Data::pDevice11->GetImmediateContext(&Data::pContext11);
        EnsureWndProcForSwapChain(pSwapChain);
        DisableDXGIAltEnter(pSwapChain);

        InitImGuiStyle();
        ImGui_ImplWin32_Init(Data::hWindow);
        ImGui_ImplDX11_Init(Data::pDevice11, Data::pContext11);
        CreateRenderTarget11(pSwapChain);
        Data::bIsInitialized = true;
        return true;
    }

    static void BeginFrame(Api api)
    {
        // Keep runtime fullscreen state in sync (DXGI) and restore exclusive fullscreen when configured.
        WindowedMode::TickDXGIState();

        if (WindowedMode::ShouldHandle())
            WindowedMode::Apply(Data::hWindow);

        Data::bIsRendering = true;

        if (api == Api::D3D10)
            ImGui_ImplDX10_NewFrame();
        else
            ImGui_ImplDX11_NewFrame();

        Data::bCallingImGui = true;
        ImGui_ImplWin32_NewFrame();
        Data::bCallingImGui = false;

        Hooks::ApplyBufferedInput();
        ImGui::NewFrame();

        ImGui::GetIO().MouseDrawCursor = Data::bShowMenu;
    }

    static void EndAndRender(Api api)
    {
        ImGui::EndFrame();
        ImGui::Render();

        if (api == Api::D3D10)
        {
            // --- State Backup ---
            ComPtr<ID3D10RenderTargetView> pOldRTV;
            ComPtr<ID3D10DepthStencilView> pOldDSV;
            Data::pDevice10->OMGetRenderTargets(1, pOldRTV.ReleaseAndGetAddressOf(), pOldDSV.ReleaseAndGetAddressOf());

            UINT nViewPorts = 1;
            D3D10_VIEWPORT pOldViewPorts[D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
            Data::pDevice10->RSGetViewports(&nViewPorts, pOldViewPorts);

            // --- Render ---
            Data::pDevice10->OMSetRenderTargets(1, &Data::pMainRenderTargetView10, NULL);
            ImGui_ImplDX10_RenderDrawData(ImGui::GetDrawData());

            // --- Restore ---
            ID3D10RenderTargetView* oldRtvRaw = pOldRTV.Get();
            Data::pDevice10->OMSetRenderTargets(1, &oldRtvRaw, pOldDSV.Get());
            Data::pDevice10->RSSetViewports(nViewPorts, pOldViewPorts);
        }
        else
        {
            // --- State Backup ---
            ComPtr<ID3D11RenderTargetView> pOldRTV;
            ComPtr<ID3D11DepthStencilView> pOldDSV;
            Data::pContext11->OMGetRenderTargets(1, pOldRTV.ReleaseAndGetAddressOf(), pOldDSV.ReleaseAndGetAddressOf());

            UINT nViewPorts = 1;
            D3D11_VIEWPORT pOldViewPorts[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
            Data::pContext11->RSGetViewports(&nViewPorts, pOldViewPorts);

            // --- Render ---
            Data::pContext11->OMSetRenderTargets(1, &Data::pMainRenderTargetView11, NULL);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

            // --- Restore ---
            ID3D11RenderTargetView* oldRtvRaw = pOldRTV.Get();
            Data::pContext11->OMSetRenderTargets(1, &oldRtvRaw, pOldDSV.Get());
            Data::pContext11->RSSetViewports(nViewPorts, pOldViewPorts);
        }

        Data::bIsRendering = false;
    }

    HRESULT Present(Api api, IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
    {
        if (Data::bIsDetached)
            return Data::oPresent(pSwapChain, SyncInterval, Flags);

        if (!EnsureInitialized(api, pSwapChain))
            return Data::oPresent(pSwapChain, SyncInterval, Flags);

        if (Data::bIsInitialized)
        {
            BeginFrame(api);

            if (Data::pSettings)
            {
                Data::pSettings->DrawOverlay();
                if (Data::bShowMenu)
                    Data::pSettings->DrawMenu();
            }

            EndAndRender(api);
        }

        g_FramerateLimiter.Wait();

        return Data::oPresent(pSwapChain, SyncInterval, Flags);
    }

    HRESULT ResizeBuffers(Api api, IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
    {
        WindowedMode::CheckAndApplyPendingState();
        if (WindowedMode::ShouldHandle() || WindowedMode::ShouldApplyResolutionOverride())
        {
            // Fix: If game requests auto-size (0,0) but we are in ScaleContent mode,
            // we must enforce the virtual resolution to prevent the swapchain from snapping to the window size.
            if (Width == 0 && Height == 0 && WindowedMode::g_State.resizeBehavior == WindowedMode::ResizeBehavior::ScaleContent)
            {
                 Width = WindowedMode::g_State.virtualWidth;
                 Height = WindowedMode::g_State.virtualHeight;
            }

            if (WindowedMode::g_State.overrideWidth > 0 && WindowedMode::g_State.overrideHeight > 0)
            {
                Width = WindowedMode::g_State.overrideWidth;
                Height = WindowedMode::g_State.overrideHeight;
            }

            LOG_INFO("DXGI: ResizeBuffers to %dx%d", Width, Height);
            if (Width > 0 && Height > 0)
                WindowedMode::NotifyResolutionChange(Width, Height);
            if (WindowedMode::ShouldHandle())
                WindowedMode::Apply(Data::hWindow);
        }

        if (Data::bIsInitialized)
        {
            if (api == Api::D3D10)
            {
                CleanupRenderTarget10();
                ImGui_ImplDX10_InvalidateDeviceObjects();
            }
            else
            {
                CleanupRenderTarget11();
                ImGui_ImplDX11_InvalidateDeviceObjects();
            }
        }

        HRESULT hr = Data::oResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);

        if (SUCCEEDED(hr))
        {
            DXGI_SWAP_CHAIN_DESC desc;
            if (SUCCEEDED(pSwapChain->GetDesc(&desc)))
            {
                if (desc.BufferDesc.Width > 0 && desc.BufferDesc.Height > 0)
                {
                        WindowedMode::NotifyResolutionChange(desc.BufferDesc.Width, desc.BufferDesc.Height);
                }
            }
        }

        if (SUCCEEDED(hr) && Data::bIsInitialized)
        {
            if (api == Api::D3D10)
            {
                CreateRenderTarget10(pSwapChain);
                ImGui_ImplDX10_CreateDeviceObjects();
            }
            else
            {
                CreateRenderTarget11(pSwapChain);
                ImGui_ImplDX11_CreateDeviceObjects();
            }
        }

        return hr;
    }

    HRESULT ResizeTarget(Api api, IDXGISwapChain* pSwapChain, const DXGI_MODE_DESC* pNewTargetParameters)
    {
        WindowedMode::CheckAndApplyPendingState();

        const bool shouldHandle = WindowedMode::ShouldHandle();
        const bool shouldOverride = WindowedMode::ShouldApplyResolutionOverride();

        if (shouldHandle || shouldOverride)
        {
             if (pNewTargetParameters)
             {
                 LOG_INFO("DXGI: ResizeTarget to %dx%d", pNewTargetParameters->Width, pNewTargetParameters->Height);
                 // If the game is requesting a specific target size, we treat it as a resolution change request
                 if (pNewTargetParameters->Width > 0 && pNewTargetParameters->Height > 0)
                     WindowedMode::NotifyResolutionChange(pNewTargetParameters->Width, pNewTargetParameters->Height);
             }
             
             if (shouldHandle)
             {
                 WindowedMode::Apply(Data::hWindow);
                 return S_OK; // Block original to prevent fighting
             }
             else if (shouldOverride)
             {
                 // In Exclusive Fullscreen with Override, we must forward the call with the overridden parameters
                 // so the swapchain/monitor switches to the desired resolution.
                 DXGI_MODE_DESC desc = (pNewTargetParameters) ? *pNewTargetParameters : DXGI_MODE_DESC{};

                 if (WindowedMode::g_State.overrideWidth > 0) desc.Width = WindowedMode::g_State.overrideWidth;
                 if (WindowedMode::g_State.overrideHeight > 0) desc.Height = WindowedMode::g_State.overrideHeight;

                 // Clear refresh rate constraints to avoid mode enumeration failure with custom resolutions
                 desc.RefreshRate.Numerator = 0;
                 desc.RefreshRate.Denominator = 0;

                 return Data::oResizeTarget(pSwapChain, &desc);
             }
        }
        
        return Data::oResizeTarget(pSwapChain, pNewTargetParameters);
    }
}