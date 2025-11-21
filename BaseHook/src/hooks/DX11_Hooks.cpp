#include "pch.h"
#include "base.h"

namespace BaseHook
{
    namespace Hooks
    {
        void CreateRenderTarget11(IDXGISwapChain* pSwapChain)
        {
            ID3D11Texture2D* pBackBuffer = nullptr;
            pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
            if (pBackBuffer)
            {
                Data::pDevice11->CreateRenderTargetView(pBackBuffer, NULL, &Data::pMainRenderTargetView11);
                pBackBuffer->Release();
            }
        }

        void CleanupRenderTarget11()
        {
            if (Data::pMainRenderTargetView11) { Data::pMainRenderTargetView11->Release(); Data::pMainRenderTargetView11 = NULL; }
        }

        HRESULT __stdcall hkPresentDX11(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
        {
            if (Data::bIsDetached) return Data::oPresent(pSwapChain, SyncInterval, Flags);

            if (!Data::bIsInitialized)
            {
                if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&Data::pDevice11)))
                {
                    Data::pDevice11->GetImmediateContext(&Data::pContext11);
                    
                    DXGI_SWAP_CHAIN_DESC desc;
                    pSwapChain->GetDesc(&desc);

                    if (desc.OutputWindow && desc.OutputWindow != Data::hWindow)
                    {
                        LOG_INFO("Window Changed (DX11). Re-hooking WndProc.");
                        RestoreWndProc(); // Safely unhook old window first
                        Data::hWindow = desc.OutputWindow;
                        Data::oWndProc = (WndProc_t)SetWindowLongPtr(Data::hWindow, GWLP_WNDPROC, (LONG_PTR)Data::pSettings->m_WndProc);
                    }

                    InitImGuiStyle();
                    ImGui_ImplWin32_Init(Data::hWindow);
                    ImGui_ImplDX11_Init(Data::pDevice11, Data::pContext11);

                    CreateRenderTarget11(pSwapChain);
                    Data::bIsInitialized = true;
                }
            }

            if (Data::bIsInitialized)
            {
                Data::bIsRendering = true;
                Hooks::ApplyBufferedInput(); // Apply thread-safe input
                ImGui_ImplDX11_NewFrame();
                ImGui_ImplWin32_NewFrame();
                ImGui::NewFrame();
                
                ImGui::GetIO().MouseDrawCursor = Data::bShowMenu;

                if (Data::pSettings)
                {
                    Data::pSettings->DrawOverlay();
                    if (Data::bShowMenu)
                        Data::pSettings->DrawMenu();
                }

                ImGui::EndFrame();
                ImGui::Render();

                // --- State Backup ---
                ID3D11RenderTargetView* pOldRTV = nullptr;
                ID3D11DepthStencilView* pOldDSV = nullptr;
                Data::pContext11->OMGetRenderTargets(1, &pOldRTV, &pOldDSV);
                
                UINT nViewPorts = 1;
                D3D11_VIEWPORT pOldViewPorts[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
                Data::pContext11->RSGetViewports(&nViewPorts, pOldViewPorts);

                // --- Render ---
                Data::pContext11->OMSetRenderTargets(1, &Data::pMainRenderTargetView11, NULL);
                ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
                
                // --- State Restore ---
                Data::pContext11->OMSetRenderTargets(1, &pOldRTV, pOldDSV);
                Data::pContext11->RSSetViewports(nViewPorts, pOldViewPorts);

                if(pOldRTV) pOldRTV->Release();
                if(pOldDSV) pOldDSV->Release();

                Data::bIsRendering = false;
            }

            return Data::oPresent(pSwapChain, SyncInterval, Flags);
        }

        HRESULT __stdcall hkResizeBuffersDX11(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
        {
            if (Data::bIsInitialized)
            {
                CleanupRenderTarget11();
                ImGui_ImplDX11_InvalidateDeviceObjects();
            }
            
            HRESULT hr = Data::oResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);

            if (SUCCEEDED(hr) && Data::bIsInitialized)
            {
                CreateRenderTarget11(pSwapChain);
                ImGui_ImplDX11_CreateDeviceObjects();
            }
            return hr;
        }
    }
}
