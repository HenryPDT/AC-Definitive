#include "pch.h"
#include "DXGI_Common.h"

// #include "WindowedMode.h" // Removed for rebase
#include "log.h"
#include "../util/ComPtr.h"

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx10.h"
#include "imgui_impl_dx11.h"

namespace BaseHook::Hooks::DXGICommon
{
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

    static bool EnsureInitialized(Api api, IDXGISwapChain* pSwapChain)
    {
        if (Data::bIsInitialized)
            return true;

        Data::pSwapChain = pSwapChain;

        if (api == Api::D3D10)
        {
            if (FAILED(pSwapChain->GetDevice(__uuidof(ID3D10Device), (void**)&Data::pDevice10)))
                return false;

            EnsureWndProcForSwapChain(pSwapChain);

            InitImGuiStyle();
            ImGui_ImplWin32_Init(Data::hWindow);
            ImGui_ImplDX10_Init(Data::pDevice10);
            CreateRenderTarget10(pSwapChain);
            Data::bIsInitialized = true;
            return true;
        }

        // D3D11
        if (FAILED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&Data::pDevice11)))
            return false;

        Data::pDevice11->GetImmediateContext(&Data::pContext11);
        EnsureWndProcForSwapChain(pSwapChain);

        InitImGuiStyle();
        ImGui_ImplWin32_Init(Data::hWindow);
        ImGui_ImplDX11_Init(Data::pDevice11, Data::pContext11);
        CreateRenderTarget11(pSwapChain);
        Data::bIsInitialized = true;
        return true;
    }

    static void BeginFrame(Api api)
    {
        // if (WindowedMode::ShouldHandle())
        //    WindowedMode::Apply(Data::hWindow);

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

        return Data::oPresent(pSwapChain, SyncInterval, Flags);
    }

    HRESULT ResizeBuffers(Api api, IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
    {
        /*
        WindowedMode::CheckAndApplyPendingState();
        if (WindowedMode::ShouldHandle())
        {
            LOG_INFO("DXGI ResizeBuffers: %dx%d", Width, Height);
            if (Width > 0 && Height > 0)
                WindowedMode::NotifyResolutionChange(Width, Height);
            WindowedMode::Apply(Data::hWindow);
        }
        */

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
}