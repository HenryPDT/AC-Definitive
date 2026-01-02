#include "pch.h"
#include "base.h"
#include "WindowedMode.h"
#include "FramerateLimiter.h"
#include "ComPtr.h"

namespace BaseHook
{
    namespace Hooks
    {
        HRESULT __stdcall hkTestCooperativeLevel(LPDIRECT3DDEVICE9 pDevice)
        {
            if (Data::g_fakeResetState == BaseHook::FakeResetState::Initiate)
            {
                LOG_INFO("hkTestCooperativeLevel: Fake Reset Initiate -> Respond (DEVICELOST)");
                Data::g_fakeResetState = BaseHook::FakeResetState::Respond;
                return D3DERR_DEVICELOST;
            }
            else if (Data::g_fakeResetState == BaseHook::FakeResetState::Respond)
            {
                // LOG_INFO("hkTestCooperativeLevel: Fake Reset Respond (DEVICENOTRESET)"); // Spammy?
                return D3DERR_DEVICENOTRESET;
            }

            HRESULT hr = Data::oTestCooperativeLevel(pDevice);
            if (hr != D3D_OK) {
                 static HRESULT lastHr = D3D_OK;
                 if (hr != lastHr) {
                     LOG_INFO("hkTestCooperativeLevel: Result=%08X, pDevice=%p", hr, pDevice);
                     
                     // Diagnostic: Check if device is Windowed or Exclusive
                     D3DDEVICE_CREATION_PARAMETERS cp;
                     if (SUCCEEDED(pDevice->GetCreationParameters(&cp))) {
                         LOG_INFO("  Device Creation Params: hFocusWindow=%p, BehaviorFlags=%08X", cp.hFocusWindow, cp.BehaviorFlags);
                     }
                     // Unfortunately GetCreationParameters doesn't return PP.Windowed.
                     // But we can check if swap chain is windowed?
                     ComPtr<IDirect3DSwapChain9> pSwap;
                     if (SUCCEEDED(pDevice->GetSwapChain(0, pSwap.GetAddressOf()))) {
                         D3DPRESENT_PARAMETERS pp;
                         if (SUCCEEDED(pSwap->GetPresentParameters(&pp))) {
                             LOG_INFO("  Swap Chain 0 PP: Windowed=%d, BackBuffer=%dx%d, hDeviceWindow=%p", pp.Windowed, pp.BackBufferWidth, pp.BackBufferHeight, pp.hDeviceWindow);
                         }
                     }

                     lastHr = hr;
                 }
            }
            return hr;
        }

        HRESULT __stdcall hkPresent9(LPDIRECT3DDEVICE9 pDevice, CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion)
        {
            if (Data::g_fakeResetState == BaseHook::FakeResetState::Initiate)
            {
                LOG_INFO("hkPresent9: Fake Reset Initiate (DEVICELOST)");
                return D3DERR_DEVICELOST;
            }

            // We don't render ImGui here because we hook EndScene for that in DX9.
            // Present just needs to pass through or fail if resetting.

            g_FramerateLimiter.Wait();

            HRESULT hr = Data::oPresent9(pDevice, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
            if (hr == D3DERR_DEVICELOST) {
                 static bool s_loggedLostPresent = false;
                 if (!s_loggedLostPresent) {
                     LOG_WARN("hkPresent9: Result=DEVICELOST");
                     s_loggedLostPresent = true;
                 }
            }
            return hr;
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
            // LOG_INFO("hkEndScene: Called"); // Very spammy, use with caution or counter
            static int logCount = 0;
            if (logCount < 5) {
                LOG_INFO("hkEndScene: Called (First 5 logs)");
                logCount++;
            }

            if (Data::bIsDetached)
            {
                return Data::oEndScene(pDevice);
            }



            if (!Data::bIsInitialized)
            {
                LOG_INFO("hkEndScene: Initializing ImGui...");
                Data::pDevice = pDevice;
                InitImGui(pDevice);
                Data::bIsInitialized = true;
                LOG_INFO("hkEndScene: ImGui Initialized.");
            }

            Data::bIsRendering = true;

            WindowedMode::TickDX9State();

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
            LOG_INFO("hkReset: Called. pDevice=%p", pDevice);
            if (pPresentationParameters) {
                LOG_INFO("  Params: BackBuffer=%dx%d, Windowed=%d, hDeviceWindow=%p",
                    pPresentationParameters->BackBufferWidth, pPresentationParameters->BackBufferHeight,
                    pPresentationParameters->Windowed, pPresentationParameters->hDeviceWindow);
            }

            WindowedMode::CheckAndApplyPendingState();

            // CRITICAL: Make a local copy. Do NOT modify the game's pointer directly.
            // Games often reuse this struct, and modifying it persists the windowed state.
            D3DPRESENT_PARAMETERS params = *pPresentationParameters;
            D3DPRESENT_PARAMETERS* pParamsToUse = pPresentationParameters;

            if (pPresentationParameters)
            {
                if (WindowedMode::ShouldHandle() && params.BackBufferWidth == 0 && params.BackBufferHeight == 0 && WindowedMode::g_State.resizeBehavior == WindowedMode::ResizeBehavior::ScaleContent)
                {
                    params.BackBufferWidth = WindowedMode::g_State.virtualWidth;
                    params.BackBufferHeight = WindowedMode::g_State.virtualHeight;
                }

                if ((WindowedMode::ShouldHandle() || WindowedMode::ShouldApplyResolutionOverride()) && WindowedMode::g_State.overrideWidth > 0 && WindowedMode::g_State.overrideHeight > 0)
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
                else if (WindowedMode::g_State.activeMode == WindowedMode::Mode::ExclusiveFullscreen) {
                    // Explicitly enforce Exclusive Fullscreen if requested, overwriting game's stale state
                    params.Windowed = FALSE;
                }

                // Always use our local copy if we modified anything or if we just want to be safe
                pParamsToUse = &params;
            }

            // Invalidate BEFORE calling original Reset
            if (Data::bIsInitialized)
                ImGui_ImplDX9_InvalidateDeviceObjects();
                
            HRESULT hr = Data::oReset(pDevice, pParamsToUse);
            LOG_INFO("hkReset: Result=%08X", hr);
            
            if (SUCCEEDED(hr))
            {
                if (pPresentationParameters && pParamsToUse == &params) {
                    *pPresentationParameters = params;
                }

                WindowedMode::UpdateDetectedStateDX9(!params.Windowed);

                ComPtr<IDirect3DSurface9> pBackBuffer;
                if (SUCCEEDED(pDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, pBackBuffer.GetAddressOf()))) {
                    D3DSURFACE_DESC desc;
                    pBackBuffer->GetDesc(&desc);
                    LOG_INFO("  Reset BackBuffer Size: %dx%d", desc.Width, desc.Height);
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

                if (WindowedMode::ShouldHandle())
                    WindowedMode::Apply(Data::hWindow);
            }
                
            return hr;
        }

        HRESULT __stdcall hkResetEx(IDirect3DDevice9Ex* pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters, D3DDISPLAYMODEEX* pFullscreenDisplayMode)
        {
            LOG_INFO("hkResetEx: Called. pDevice=%p", pDevice);
            if (pPresentationParameters) {
                LOG_INFO("  Params: BackBuffer=%dx%d, Windowed=%d, hDeviceWindow=%p",
                    pPresentationParameters->BackBufferWidth, pPresentationParameters->BackBufferHeight,
                    pPresentationParameters->Windowed, pPresentationParameters->hDeviceWindow);
            }

            WindowedMode::CheckAndApplyPendingState();

            D3DPRESENT_PARAMETERS params = *pPresentationParameters;
            D3DPRESENT_PARAMETERS* pParamsToUse = pPresentationParameters;
            D3DDISPLAYMODEEX* pFullscreenModeToUse = pFullscreenDisplayMode;

            if (pPresentationParameters)
            {
                if (WindowedMode::ShouldHandle() && params.BackBufferWidth == 0 && params.BackBufferHeight == 0 && WindowedMode::g_State.resizeBehavior == WindowedMode::ResizeBehavior::ScaleContent)
                {
                    params.BackBufferWidth = WindowedMode::g_State.virtualWidth;
                    params.BackBufferHeight = WindowedMode::g_State.virtualHeight;
                }

                if ((WindowedMode::ShouldHandle() || WindowedMode::ShouldApplyResolutionOverride()) && WindowedMode::g_State.overrideWidth > 0 && WindowedMode::g_State.overrideHeight > 0)
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
                else if (WindowedMode::g_State.activeMode == WindowedMode::Mode::ExclusiveFullscreen) {
                    params.Windowed = FALSE;
                }

                pParamsToUse = &params;
            }

            // Invalidate BEFORE calling original ResetEx
            if (Data::bIsInitialized)
                ImGui_ImplDX9_InvalidateDeviceObjects();

            HRESULT hr = Data::oResetEx(pDevice, pParamsToUse, pFullscreenModeToUse);
            LOG_INFO("hkResetEx: Result=%08X", hr);

            if (SUCCEEDED(hr))
            {
                if (pPresentationParameters && pParamsToUse == &params) {
                    *pPresentationParameters = params;
                }

                WindowedMode::UpdateDetectedStateDX9(!params.Windowed);

                ComPtr<IDirect3DSurface9> pBackBuffer;
                if (SUCCEEDED(pDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, pBackBuffer.GetAddressOf()))) {
                    D3DSURFACE_DESC desc;
                    pBackBuffer->GetDesc(&desc);
                    LOG_INFO("  ResetEx BackBuffer Size: %dx%d", desc.Width, desc.Height);
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

                if (WindowedMode::ShouldHandle())
                    WindowedMode::Apply(Data::hWindow);
            }

            return hr;
        }
    }
}
