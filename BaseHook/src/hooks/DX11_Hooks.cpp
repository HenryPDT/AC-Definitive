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
                    DXGI_SWAP_CHAIN_DESC desc;
                    pSwapChain->GetDesc(&desc);

                    Data::pDevice11->GetImmediateContext(&Data::pContext11);

                    if (desc.OutputWindow && desc.OutputWindow != Data::hWindow)
                    {
                        LOG_INFO("Window mismatch detected (Hooked: %p, DXGI: %p). Re-hooking WndProc.", Data::hWindow, desc.OutputWindow);
                        if (Data::hWindow && Data::oWndProc) SetWindowLongPtr(Data::hWindow, GWLP_WNDPROC, (LONG_PTR)Data::oWndProc);
                        Data::hWindow = desc.OutputWindow;
                        Data::oWndProc = (WndProc_t)SetWindowLongPtr(Data::hWindow, GWLP_WNDPROC, (LONG_PTR)Data::pSettings->m_WndProc);
                    }

                    InitImGuiStyle();
                    ImGui_ImplWin32_Init(desc.OutputWindow);
                    ImGui_ImplDX11_Init(Data::pDevice11, Data::pContext11);

                    CreateRenderTarget11(pSwapChain);
                    Data::bIsInitialized = true;
                }
            }

            if (Data::bIsInitialized)
            {
                ImGui_ImplDX11_NewFrame();
                ImGui_ImplWin32_NewFrame();
                ImGui::NewFrame();

                ImGuiLayer_EvenWhenMenuIsClosed();
                if (Data::bShowMenu)
                {
                    ImGuiLayer_WhenMenuIsOpen();
                }

                ImGui::EndFrame();
                ImGui::Render();
                Data::pContext11->OMSetRenderTargets(1, &Data::pMainRenderTargetView11, NULL);
                ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            }

            return Data::oPresent(pSwapChain, SyncInterval, Flags);
        }

        HRESULT __stdcall hkResizeBuffersDX11(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
        {
            if (Data::bIsInitialized)
            {
                CleanupRenderTarget11();
            }

            HRESULT hr = Data::oResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);

            if (Data::bIsInitialized && SUCCEEDED(hr))
            {
                CreateRenderTarget11(pSwapChain);
            }
            return hr;
        }
    }
}