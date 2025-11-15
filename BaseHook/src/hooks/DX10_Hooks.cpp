#include "pch.h"
#include "base.h"

namespace BaseHook
{
    namespace Hooks
    {
        void CreateRenderTarget(IDXGISwapChain* pSwapChain)
        {
            ID3D10Texture2D* pBackBuffer;
            pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
            if (pBackBuffer)
            {
                Data::pDevice10->CreateRenderTargetView(pBackBuffer, NULL, &Data::pMainRenderTargetView10);
                pBackBuffer->Release();
            }
        }

        void CleanupRenderTarget()
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
                        LOG_INFO("Window mismatch detected (Hooked: %p, DXGI: %p). Re-hooking WndProc.", Data::hWindow, desc.OutputWindow);
                        if (Data::hWindow && Data::oWndProc) SetWindowLongPtr(Data::hWindow, GWLP_WNDPROC, (LONG_PTR)Data::oWndProc);
                        Data::hWindow = desc.OutputWindow;
                        Data::oWndProc = (WndProc_t)SetWindowLongPtr(Data::hWindow, GWLP_WNDPROC, (LONG_PTR)Data::pSettings->m_WndProc);
                    }

                    InitImGuiStyle();
                    ImGui_ImplWin32_Init(desc.OutputWindow);
                    ImGui_ImplDX10_Init(Data::pDevice10);

                    CreateRenderTarget(pSwapChain);
                    Data::bIsInitialized = true;
                }
            }

            if (Data::bIsInitialized)
            {
                ImGui_ImplDX10_NewFrame();
                ImGui_ImplWin32_NewFrame();
                ImGui::NewFrame();

                ImGuiLayer_EvenWhenMenuIsClosed();
                if (Data::bShowMenu)
                {
                    ImGuiLayer_WhenMenuIsOpen();
                }

                ImGui::EndFrame();
                ImGui::Render();
                Data::pDevice10->OMSetRenderTargets(1, &Data::pMainRenderTargetView10, NULL);
                ImGui_ImplDX10_RenderDrawData(ImGui::GetDrawData());
            }

            return Data::oPresent(pSwapChain, SyncInterval, Flags);
        }

        HRESULT __stdcall hkResizeBuffersDX10(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
        {
            if (Data::bIsInitialized)
            {
                CleanupRenderTarget();
            }
            
            HRESULT hr = Data::oResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
            
            if (Data::bIsInitialized && SUCCEEDED(hr))
            {
                CreateRenderTarget(pSwapChain);
            }
            return hr;
        }
    }
}