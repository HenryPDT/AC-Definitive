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
            // Initialize MinHook
            if (MH_Initialize() != MH_OK && MH_Initialize() != MH_ERROR_ALREADY_INITIALIZED)
            {
                LOG_ERROR("InstallEarlyHooks: MinHook initialization failed.");
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

            // Check if hooks are bound (manually via D3DCreateHooks)
            bool isD3D9Hooked = (Data::oEndScene != nullptr);
            bool isD3D11Hooked = (Data::oPresent != nullptr); // DX10 uses same pointer variable usually or distinct?
            // base.h: Present_t oPresent; used for DX10/11.

            if (!isD3D9Hooked && !isD3D11Hooked) {
                // Not ready yet. D3DCreateHooks will call us again when device is created.
                return;
            }

            Data::bGraphicsInitialized = true;
            LOG_INFO("FinishInitialization: Graphics initialized.");
        }

        static BOOL CALLBACK EnumWindowsCallback(HWND handle, LPARAM lParam)
        {
            DWORD wndProcId;
            GetWindowThreadProcessId(handle, &wndProcId);

            if (GetCurrentProcessId() != wndProcId)
                return TRUE;

            if (!IsWindowVisible(handle)) {
                return TRUE;
            }

            char className[256];
            if (GetClassNameA(handle, className, sizeof(className)) == 0) return TRUE;
            
            // Simple heuristic for game window: has a non-zero area client region
            RECT rect;
            if (!GetClientRect(handle, &rect)) return TRUE;

            int w = rect.right - rect.left;
            int h = rect.bottom - rect.top;

            if (strcmp(className, "ConsoleWindowClass") == 0 || w <= 32 || h <= 32) {
                 return TRUE;
            }

            Data::hWindow = handle;
            LOG_INFO("EnumWindows: Found window %p (%s)", handle, className);
            return FALSE; // Stop enumeration
        }

        bool Init()
        {
            // Double check MinHook init (in case EarlyHooks wasn't called or failed)
            if (MH_Initialize() != MH_OK && MH_Initialize() != MH_ERROR_ALREADY_INITIALIZED) {
                LOG_ERROR("Failed to initialize MinHook.");
                return false;
            }

            if (!Data::pSettings) {
                LOG_ERROR("Init: Settings not found.");
                return false;
            }

            // Hook Inputs
            InitDirectInput();
            InitXInput();

            // Determine Effective Render Type
            // Centralized: WindowedMode::EarlyInit already resolved Auto (0) to a concrete version.
            int effectiveDX = WindowedMode::g_State.targetDXVersion;
            
            // Final fallback if detection failed
            if (effectiveDX == 0)
            {
                if (GetModuleHandleA("d3d11.dll") != NULL) effectiveDX = 11;
                else if (GetModuleHandleA("d3d10.dll") != NULL) effectiveDX = 10;
                else if (GetModuleHandleA("d3d9.dll") != NULL) effectiveDX = 9;

                if (effectiveDX != 0) {
                    WindowedMode::g_State.targetDXVersion = effectiveDX;
                }
            }

            if (effectiveDX != 0)
            {
                LOG_INFO("Init: Effective DirectX Version: %d", effectiveDX);
            }

            // Late-install hooks
            if (effectiveDX == 9) {
                BaseHook::WindowedMode::InstallD3D9HooksLate();
            }
            else if (effectiveDX == 10 || effectiveDX == 11) {
                BaseHook::WindowedMode::InstallDXGIHooksLate();
            }

            // Ensure Windowed Mode hooks are applied (idempotent)
            BaseHook::WindowedMode::InstallHooks();

            // If we didn't find a window during early hook phase, try to find it now
            if (Data::hWindow == NULL)
            {
                EnumWindows(EnumWindowsCallback, 0);
                if (Data::hWindow) {
                    if (BaseHook::WindowedMode::ShouldHandle()) {
                        BaseHook::WindowedMode::Apply(Data::hWindow);
                    }
                }
            }

            // Always ensure WndProc is hooked (if window exists)
            if (Data::hWindow) {
                InstallWndProcHook();
            }

            LOG_INFO("Init: Basehook initialized.");
            return true;
        }
        
        void InstallWndProcHook()
        {
            if (!Data::hWindow) return;

            if (Data::oWndProc == nullptr) {
                 Data::oWndProc = (WndProc_t)SetWindowLongPtr(Data::hWindow, GWLP_WNDPROC, (LONG_PTR)Data::pSettings->m_WndProc);
                 LOG_INFO("InstallWndProcHook: Hooked window %p.", Data::hWindow);
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
            
            // Disable all hooks (since we bypassed kiero::shutdown logic)
            MH_DisableHook(MH_ALL_HOOKS);
            
            kiero::shutdown();
        }
    }
}
