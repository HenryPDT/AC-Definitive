#include "pch.h"
#include "base.h"

namespace BaseHook
{
    namespace Hooks
    {
        void CreateRenderTarget10(IDXGISwapChain* pSwapChain)
        {
            ID3D10Texture2D* pBackBuffer;
            pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
            if (pBackBuffer)
            {
                Data::pDevice10->CreateRenderTargetView(pBackBuffer, NULL, &Data::pMainRenderTargetView10);
                pBackBuffer->Release();
            }
        }

        void CleanupRenderTarget10()
        {
            if (Data::pMainRenderTargetView10) { Data::pMainRenderTargetView10->Release(); Data::pMainRenderTargetView10 = NULL; }
        }

        HRESULT __stdcall hkPresentDX10(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
        {
            if (Data::bIsDetached) return Data::oPresent(pSwapChain, SyncInterval, Flags);

            if (!Data::bIsInitialized)
            {
                if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D10Device), (void**)&Data::pDevice10)))
                {
                    DXGI_SWAP_CHAIN_DESC desc;
                    pSwapChain->GetDesc(&desc);

                    if (desc.OutputWindow && desc.OutputWindow != Data::hWindow)
                    {
                        LOG_INFO("Window Changed (DX10). Re-hooking WndProc.");
                        RestoreWndProc(); // Safely unhook old window first
                        Data::hWindow = desc.OutputWindow;
                        Data::oWndProc = (WndProc_t)SetWindowLongPtr(Data::hWindow, GWLP_WNDPROC, (LONG_PTR)Data::pSettings->m_WndProc);
                    }

                    InitImGuiStyle();
                    ImGui_ImplWin32_Init(Data::hWindow);
                    ImGui_ImplDX10_Init(Data::pDevice10);

                    CreateRenderTarget10(pSwapChain);
                    Data::bIsInitialized = true;
                }
            }

            if (Data::bIsInitialized)
            {
                Data::bIsRendering = true;
                ImGui_ImplDX10_NewFrame();

                Data::bCallingImGui = true;
                ImGui_ImplWin32_NewFrame();
                Data::bCallingImGui = false;

                Hooks::ApplyBufferedInput(); // Apply thread-safe input after backend updates
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
                ID3D10RenderTargetView* pOldRTV = nullptr;
                ID3D10DepthStencilView* pOldDSV = nullptr;
                Data::pDevice10->OMGetRenderTargets(1, &pOldRTV, &pOldDSV);
                
                UINT nViewPorts = 1;
                D3D10_VIEWPORT pOldViewPorts[D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
                Data::pDevice10->RSGetViewports(&nViewPorts, pOldViewPorts);

                // --- Render ---
                Data::pDevice10->OMSetRenderTargets(1, &Data::pMainRenderTargetView10, NULL);
                ImGui_ImplDX10_RenderDrawData(ImGui::GetDrawData());
                
                // --- State Restore ---
                Data::pDevice10->OMSetRenderTargets(1, &pOldRTV, pOldDSV);
                Data::pDevice10->RSSetViewports(nViewPorts, pOldViewPorts);

                if(pOldRTV) pOldRTV->Release();
                if(pOldDSV) pOldDSV->Release();

                Data::bIsRendering = false;
            }

            return Data::oPresent(pSwapChain, SyncInterval, Flags);
        }

        HRESULT __stdcall hkResizeBuffersDX10(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
        {
            if (Data::bIsInitialized)
            {
                CleanupRenderTarget10();
                ImGui_ImplDX10_InvalidateDeviceObjects();
            }
            
            HRESULT hr = Data::oResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
            
            if (SUCCEEDED(hr) && Data::bIsInitialized)
            {
                CreateRenderTarget10(pSwapChain);
                ImGui_ImplDX10_CreateDeviceObjects();
            }
            return hr;
        }
    }
}
