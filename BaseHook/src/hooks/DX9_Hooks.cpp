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
                RestoreWndProc(); // Safely unhook old window first
                Data::hWindow = params.hFocusWindow;
                Data::oWndProc = (WndProc_t)SetWindowLongPtr(Data::hWindow, GWLP_WNDPROC, (LONG_PTR)Data::pSettings->m_WndProc);
            }

            InitImGuiStyle();
            ImGui_ImplWin32_Init(Data::hWindow);
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

            Data::bIsRendering = true;
            ImGui_ImplDX9_NewFrame();

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
            
            // DX9 saves state internally in ImGui_ImplDX9 usually, but 
            // user app should be careful about state blocks. 
            // Standard ImGui_ImplDX9 is usually robust enough for EndScene.
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
            
            Data::bIsRendering = false;

            return Data::oEndScene(pDevice);
        }

        HRESULT __stdcall hkReset(LPDIRECT3DDEVICE9 pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters)
        {
            // Invalidate BEFORE calling original Reset
            if (Data::bIsInitialized)
                ImGui_ImplDX9_InvalidateDeviceObjects();
                
            HRESULT hr = Data::oReset(pDevice, pPresentationParameters);
            
            // Restore AFTER calling original Reset
            if (SUCCEEDED(hr) && Data::bIsInitialized)
                ImGui_ImplDX9_CreateDeviceObjects();
                
            return hr;
        }
    }
}
