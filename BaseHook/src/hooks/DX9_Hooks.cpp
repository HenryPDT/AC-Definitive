#include "pch.h"
#include "base.h"
#include "WindowedMode.h"
#include "FramerateLimiter.h"

namespace BaseHook
{
    namespace Hooks
    {
        HRESULT __stdcall hkTestCooperativeLevel(LPDIRECT3DDEVICE9 pDevice)
        {
            if (Data::g_fakeResetState == BaseHook::FakeResetState::Initiate)
            {
                Data::g_fakeResetState = BaseHook::FakeResetState::Respond;
                return D3DERR_DEVICELOST;
            }
            else if (Data::g_fakeResetState == BaseHook::FakeResetState::Respond)
            {
                return D3DERR_DEVICENOTRESET;
            }

            return Data::oTestCooperativeLevel(pDevice);
        }

        HRESULT __stdcall hkPresent9(LPDIRECT3DDEVICE9 pDevice, CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion)
        {
            if (Data::g_fakeResetState == BaseHook::FakeResetState::Initiate)
            {
                return D3DERR_DEVICELOST;
            }

            // We don't render ImGui here because we hook EndScene for that in DX9.
            // Present just needs to pass through or fail if resetting.

            g_FramerateLimiter.Wait();

            return Data::oPresent9(pDevice, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
        }

        void InitImGui(LPDIRECT3DDEVICE9 pDevice)
        {
            D3DDEVICE_CREATION_PARAMETERS params;
            pDevice->GetCreationParameters(&params);

            if (params.hFocusWindow && params.hFocusWindow != Data::hWindow)
            {
                RestoreWndProc(); // Safely unhook old window first
                Data::hWindow = params.hFocusWindow;
                InstallWndProcHook();
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
            WindowedMode::CheckAndApplyPendingState();

            if (pPresentationParameters)
            {
                WindowedMode::UpdateDetectedStateDX9(!pPresentationParameters->Windowed);
            }

            // CRITICAL: Make a local copy. Do NOT modify the game's pointer directly.
            // Games often reuse this struct, and modifying it persists the windowed state.
            D3DPRESENT_PARAMETERS params = *pPresentationParameters;
            D3DPRESENT_PARAMETERS* pParamsToUse = pPresentationParameters;

            if ((WindowedMode::ShouldHandle() || WindowedMode::ShouldApplyResolutionOverride()) && pPresentationParameters)
            {
                if (params.BackBufferWidth == 0 && params.BackBufferHeight == 0 && WindowedMode::g_State.resizeBehavior == WindowedMode::ResizeBehavior::ScaleContent)
                {
                    params.BackBufferWidth = WindowedMode::g_State.virtualWidth;
                    params.BackBufferHeight = WindowedMode::g_State.virtualHeight;
                    LOG_INFO("D3D9 Reset: Auto-size blocked by ScaleContent. Enforcing %dx%d", params.BackBufferWidth, params.BackBufferHeight);
                }

                if (WindowedMode::g_State.overrideWidth > 0 && WindowedMode::g_State.overrideHeight > 0)
                {
                    params.BackBufferWidth = WindowedMode::g_State.overrideWidth;
                    params.BackBufferHeight = WindowedMode::g_State.overrideHeight;
                }

                LOG_INFO("D3D9 Reset: Windowed=%d, Size=%dx%d", pPresentationParameters->Windowed, params.BackBufferWidth, params.BackBufferHeight);

                if (params.BackBufferWidth > 0 && params.BackBufferHeight > 0) {
                    WindowedMode::NotifyResolutionChange(params.BackBufferWidth, params.BackBufferHeight);
                }

                if (WindowedMode::ShouldHandle()) {
                    params.Windowed = TRUE;
                    params.FullScreen_RefreshRateInHz = 0;
                }
                pParamsToUse = &params;
                
                if (WindowedMode::ShouldHandle())
                    WindowedMode::Apply(Data::hWindow);
            }

            // Invalidate BEFORE calling original Reset
            if (Data::bIsInitialized)
                ImGui_ImplDX9_InvalidateDeviceObjects();
                
            HRESULT hr = Data::oReset(pDevice, pParamsToUse);
            
            if (SUCCEEDED(hr))
            {
                IDirect3DSurface9* pBackBuffer = nullptr;
                if (SUCCEEDED(pDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer))) {
                    D3DSURFACE_DESC desc;
                    pBackBuffer->GetDesc(&desc);
                    pBackBuffer->Release();
                    WindowedMode::NotifyResolutionChange(desc.Width, desc.Height);
                }
            }

            // Restore AFTER calling original Reset
            if (SUCCEEDED(hr) && Data::bIsInitialized)
            {
                ImGui_ImplDX9_CreateDeviceObjects();

                // Reset successful, clear fake state
                if (Data::g_fakeResetState != BaseHook::FakeResetState::Clear)
                {
                    Data::g_fakeResetState = BaseHook::FakeResetState::Clear;
                    LOG_INFO("Fake Device Reset Completed Successfully.");
                }
            }
                
            return hr;
        }

        HRESULT __stdcall hkResetEx(IDirect3DDevice9Ex* pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters, D3DDISPLAYMODEEX* pFullscreenDisplayMode)
        {
            WindowedMode::CheckAndApplyPendingState();

            if (pPresentationParameters)
            {
                WindowedMode::UpdateDetectedStateDX9(!pPresentationParameters->Windowed);
            }

            D3DPRESENT_PARAMETERS params = *pPresentationParameters;
            D3DPRESENT_PARAMETERS* pParamsToUse = pPresentationParameters;
            D3DDISPLAYMODEEX* pFullscreenModeToUse = pFullscreenDisplayMode;

            if ((WindowedMode::ShouldHandle() || WindowedMode::ShouldApplyResolutionOverride()) && pPresentationParameters)
            {
                if (params.BackBufferWidth == 0 && params.BackBufferHeight == 0 && WindowedMode::g_State.resizeBehavior == WindowedMode::ResizeBehavior::ScaleContent)
                {
                    params.BackBufferWidth = WindowedMode::g_State.virtualWidth;
                    params.BackBufferHeight = WindowedMode::g_State.virtualHeight;
                    LOG_INFO("D3D9Ex ResetEx: Auto-size blocked by ScaleContent. Enforcing %dx%d", params.BackBufferWidth, params.BackBufferHeight);
                }

                if (WindowedMode::g_State.overrideWidth > 0 && WindowedMode::g_State.overrideHeight > 0)
                {
                    params.BackBufferWidth = WindowedMode::g_State.overrideWidth;
                    params.BackBufferHeight = WindowedMode::g_State.overrideHeight;
                }

                LOG_INFO("D3D9Ex ResetEx: Windowed=%d, Size=%dx%d", pPresentationParameters->Windowed, params.BackBufferWidth, params.BackBufferHeight);

                if (params.BackBufferWidth > 0 && params.BackBufferHeight > 0) {
                    WindowedMode::NotifyResolutionChange(params.BackBufferWidth, params.BackBufferHeight);
                }

                if (WindowedMode::ShouldHandle()) {
                    params.Windowed = TRUE;
                    params.FullScreen_RefreshRateInHz = 0;
                    // Force pFullscreenDisplayMode to NULL in windowed mode
                    pFullscreenModeToUse = nullptr;
                }
                pParamsToUse = &params;

                if (WindowedMode::ShouldHandle())
                    WindowedMode::Apply(Data::hWindow);
            }

            // Invalidate BEFORE calling original ResetEx
            if (Data::bIsInitialized)
                ImGui_ImplDX9_InvalidateDeviceObjects();

            HRESULT hr = Data::oResetEx(pDevice, pParamsToUse, pFullscreenModeToUse);

            if (SUCCEEDED(hr))
            {
                IDirect3DSurface9* pBackBuffer = nullptr;
                if (SUCCEEDED(pDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer))) {
                    D3DSURFACE_DESC desc;
                    pBackBuffer->GetDesc(&desc);
                    pBackBuffer->Release();
                    WindowedMode::NotifyResolutionChange(desc.Width, desc.Height);
                }
            }

            // Restore AFTER calling original ResetEx
            if (SUCCEEDED(hr) && Data::bIsInitialized)
            {
                ImGui_ImplDX9_CreateDeviceObjects();

                if (Data::g_fakeResetState != BaseHook::FakeResetState::Clear)
                {
                    Data::g_fakeResetState = BaseHook::FakeResetState::Clear;
                    LOG_INFO("Fake Device ResetEx Completed Successfully.");
                }
            }

            return hr;
        }
    }
}
