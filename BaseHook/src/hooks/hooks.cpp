#include "pch.h"
#include "base.h"
#include "WindowedMode.h"
#include "hooks/D3DCreateHooks.h"
#include "GameDetection.h"
#include "RenderDetection.h"

namespace BaseHook
{
    namespace Hooks
    {
        // Internal Helpers
        void InstallWndProcHook();
        void RestoreWndProc();

        // Forward declarations of other hook files
        void InitDirectInput();
        void InitXInput();
        void CleanupDirectInput();

        static bool g_WindowHooksInstalled = false;

        // --- Lifecycle Management ---

        void InstallEarlyHooks(HMODULE hModule)
        {
            LOG_INFO("InstallEarlyHooks: Starting...");
            
            // Initialize MinHook
            if (MH_Initialize() != MH_OK && MH_Initialize() != MH_ERROR_ALREADY_INITIALIZED)
            {
                LOG_ERROR("InstallEarlyHooks: MinHook Init Failed.");
                return;
            }

            // Install Windowed Mode Hooks (User32 / D3D Creation)
            // Always install hooks to ensure we catch CreateWindowEx and get the window handle early.
            BaseHook::WindowedMode::InstallHooks();
            g_WindowHooksInstalled = true;
        }

        void FinishInitialization()
        {
            if (Data::bGraphicsInitialized) return;

            // Kiero Binding (Runtime Render Hooks)
            kiero::RenderType::Enum type = kiero::getRenderType();

            if (type == kiero::RenderType::None) return; // Not ready

            LOG_INFO("Finishing Initialization for Window: %p (RenderType: %d)", Data::hWindow, type);

            if (type == kiero::RenderType::D3D11) {
                kiero::bind(8, (void**)&Data::oPresent, hkPresentDX11);
                kiero::bind(13, (void**)&Data::oResizeBuffers, hkResizeBuffersDX11);
                kiero::bind(14, (void**)&Data::oResizeTarget, hkResizeTargetDX11);
            }
            else if (type == kiero::RenderType::D3D10) {
                kiero::bind(8, (void**)&Data::oPresent, hkPresentDX10);
                kiero::bind(13, (void**)&Data::oResizeBuffers, hkResizeBuffersDX10);
                kiero::bind(14, (void**)&Data::oResizeTarget, hkResizeTargetDX10);
            }
            else if (type == kiero::RenderType::D3D9) {
                kiero::bind(42, (void**)&Data::oEndScene, hkEndScene);
                kiero::bind(16, (void**)&Data::oReset, hkReset);
                kiero::bind(17, (void**)&Data::oPresent9, hkPresent9);
                kiero::bind(3, (void**)&Data::oTestCooperativeLevel, hkTestCooperativeLevel);
            }
            else {
                LOG_ERROR("Unsupported render type in FinishInitialization.");
                return;
            }

            Data::bGraphicsInitialized = true;
        }

        static BOOL CALLBACK EnumWindowsCallback(HWND handle, LPARAM lParam)
        {
            DWORD wndProcId;
            GetWindowThreadProcessId(handle, &wndProcId);

            if (GetCurrentProcessId() != wndProcId)
                return TRUE;

            if (!IsWindowVisible(handle))
                return TRUE;

            char className[256];
            if (GetClassNameA(handle, className, sizeof(className)) == 0) return TRUE;
            if (strcmp(className, "ConsoleWindowClass") == 0) return TRUE;

            // Simple heuristic for game window: has a non-zero area client region
            RECT rect;
            if (!GetClientRect(handle, &rect)) return TRUE;
            if ((rect.right - rect.left) <= 32 || (rect.bottom - rect.top) <= 32) return TRUE;

            Data::hWindow = handle;
            return FALSE; // Stop enumeration
        }

        bool Init()
        {
            // Double check MinHook init (in case EarlyHooks wasn't called or failed)
            if (MH_Initialize() != MH_OK && MH_Initialize() != MH_ERROR_ALREADY_INITIALIZED) {
                LOG_ERROR("Failed to initialize MinHook.");
                return false;
            }

            if (!Data::pSettings) return false;

            // Hook Inputs
            InitDirectInput();
            InitXInput();

            // Late-install D3D9 hooks (safe from Main Thread)
            // This catches cases where game created IDirect3D9 before we hooked exports in DllMain
            BaseHook::WindowedMode::InstallD3D9HooksLate();

            // Init Kiero for rendering hooks
            kiero::RenderType::Enum renderType = kiero::RenderType::None;
            int dxVer = Data::pSettings->m_DirectXVersion;
            
            if (dxVer == 9) renderType = kiero::RenderType::D3D9;
            else if (dxVer == 10) renderType = kiero::RenderType::D3D10;
            else if (dxVer == 11) renderType = kiero::RenderType::D3D11;
            else
            {
                // Auto detection
                renderType = Util::GetGameSpecificRenderType();
                if (renderType == kiero::RenderType::None)
                {
                    if (GetModuleHandleA("d3d10.dll") != NULL) renderType = kiero::RenderType::D3D10;
                    else if (GetModuleHandleA("d3d9.dll") != NULL) renderType = kiero::RenderType::D3D9;
                    else if (GetModuleHandleA("d3d11.dll") != NULL) renderType = kiero::RenderType::D3D11;
                    else renderType = kiero::RenderType::Auto;
                }
            }

            LOG_INFO("Kiero Init: Target=%d (Config=%d)", (int)renderType, dxVer);
            if (kiero::init(renderType) != kiero::Status::Success) {
                LOG_ERROR("Kiero initialization failed (Target=%d).", renderType);
                return false;
            }

            // Ensure Windowed Mode hooks are applied (idempotent)
            BaseHook::WindowedMode::InstallHooks();

            kiero::RenderType::Enum type = kiero::getRenderType();
            LOG_INFO("Render Type: %s (%d)", (type == kiero::RenderType::D3D9 ? "D3D9" : "DX10/11"), type);

            // If we didn't find a window during early hook phase, try to find it now
            if (Data::hWindow == NULL)
            {
                EnumWindows(EnumWindowsCallback, 0);
                if (Data::hWindow) {
                    LOG_INFO("Late Injection: Found window %p", Data::hWindow);
                    if (BaseHook::WindowedMode::ShouldHandle()) {
                        BaseHook::WindowedMode::Apply(Data::hWindow);
                    }
                }
            }

            // Always ensure WndProc is hooked
            InstallWndProcHook();

            return true;
        }
        
        void InstallWndProcHook()
        {
            if (!Data::hWindow) return;

            if (Data::oWndProc == nullptr) {
                 Data::oWndProc = (WndProc_t)SetWindowLongPtr(Data::hWindow, GWLP_WNDPROC, (LONG_PTR)Data::pSettings->m_WndProc);
            }

            NotifyDirectInputWindow(Data::hWindow);
            FinishInitialization();
        }

        void RestoreWndProc()
        {
            if (Data::oWndProc && Data::hWindow)
            {
                SetWindowLongPtr(Data::hWindow, GWLP_WNDPROC, (LONG_PTR)Data::oWndProc);
                Data::oWndProc = nullptr;
                LOG_INFO("WndProc Restored.");
            }
        }

        void Shutdown()
        {
            RestoreWndProc();

            if (ImGui::GetCurrentContext())
            {
                auto type = kiero::getRenderType();
                if (type == kiero::RenderType::D3D9) {
                    ImGui_ImplDX9_Shutdown();
                }
                else if (type == kiero::RenderType::D3D11) {
                    ImGui_ImplDX11_Shutdown();
                    if (Data::pMainRenderTargetView11) { Data::pMainRenderTargetView11->Release(); Data::pMainRenderTargetView11 = nullptr; }
                    if (Data::pContext11) { Data::pContext11->Release(); Data::pContext11 = nullptr; }
                    if (Data::pDevice11) { Data::pDevice11->Release(); Data::pDevice11 = nullptr; }
                }
                else if (type == kiero::RenderType::D3D10) {
                    ImGui_ImplDX10_Shutdown();
                    if (Data::pMainRenderTargetView10) { Data::pMainRenderTargetView10->Release(); Data::pMainRenderTargetView10 = nullptr; }
                    if (Data::pDevice10) { Data::pDevice10->Release(); Data::pDevice10 = nullptr; }
                }
                ImGui_ImplWin32_Shutdown();
                ImGui::DestroyContext();
            }

            CleanupDirectInput();
            kiero::shutdown();
        }
    }
}
