#include "pch.h"
#include "base.h"

namespace BaseHook
{
    namespace Hooks
    {
        void InitImGui(LPDIRECT3DDEVICE9 pDevice)
        {
            D3DDEVICE_CREATION_PARAMETERS params;
            pDevice->GetCreationParameters(&params);

            if (params.hFocusWindow && params.hFocusWindow != Data::hWindow)
            {
                LOG_INFO("Window mismatch detected (Hooked: %p, D3D: %p). Re-hooking WndProc.", Data::hWindow, params.hFocusWindow);
                if (Data::hWindow && Data::oWndProc) SetWindowLongPtr(Data::hWindow, GWLP_WNDPROC, (LONG_PTR)Data::oWndProc);
                Data::hWindow = params.hFocusWindow;
                Data::oWndProc = (WndProc_t)SetWindowLongPtr(Data::hWindow, GWLP_WNDPROC, (LONG_PTR)Data::pSettings->m_WndProc);
            }

            InitImGuiStyle();
            ImGui_ImplWin32_Init(params.hFocusWindow);
            ImGui_ImplDX9_Init(pDevice);
        }

        HRESULT __stdcall hkEndScene(LPDIRECT3DDEVICE9 pDevice)
        {
            if (Data::bIsDetached)
            {
                return Data::oEndScene(pDevice);
            }

            if (!Data::bIsInitialized)
            {
                Data::pDevice = pDevice;
                InitImGui(pDevice);
                Data::bIsInitialized = true;
            }

            ImGui_ImplDX9_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            ImGuiLayer_EvenWhenMenuIsClosed();
            if (Data::bShowMenu)
            {
                ImGuiLayer_WhenMenuIsOpen();
            }

            ImGui::EndFrame();
            ImGui::Render();
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());

            return Data::oEndScene(pDevice);
        }

        HRESULT __stdcall hkReset(LPDIRECT3DDEVICE9 pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters)
        {
            if (Data::bIsInitialized)
                ImGui_ImplDX9_InvalidateDeviceObjects();
            HRESULT hr = Data::oReset(pDevice, pPresentationParameters);
            if (hr >= 0)
            {
                if (Data::bIsInitialized)
                    ImGui_ImplDX9_CreateDeviceObjects();
            }
            return hr;
        }
    }
}