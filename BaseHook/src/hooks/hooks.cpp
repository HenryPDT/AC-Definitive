#include "pch.h"
#include "base.h"

namespace BaseHook
{
    namespace Hooks
    {
        void InitDirectInput();
        void InitXInput();

        namespace
        {
            const char* RenderTypeToString(kiero::RenderType::Enum type)
            {
                switch (type)
                {
                case kiero::RenderType::D3D9:   return "D3D9";
                case kiero::RenderType::D3D10:  return "D3D10";
                case kiero::RenderType::D3D11:  return "D3D11";
                default:                        return "Unsupported";
                }
            }

            struct WindowSearchContext {
                HWND bestHandle = nullptr;
                long maxArea = 0;
            };

            BOOL CALLBACK EnumWindowsCallback(HWND handle, LPARAM lParam)
            {
                WindowSearchContext* context = (WindowSearchContext*)lParam;

                DWORD wndProcId;
                GetWindowThreadProcessId(handle, &wndProcId);

                if (GetCurrentProcessId() != wndProcId)
                    return TRUE;

                // Ignore console windows
                char className[256];
                GetClassNameA(handle, className, sizeof(className));
                if (strcmp(className, "ConsoleWindowClass") == 0) 
                    return TRUE;

                if (handle == GetConsoleWindow())
                    return TRUE;

                if (!IsWindowVisible(handle))
                    return TRUE;

                // Get Window Title for logging
                char title[256];
                GetWindowTextA(handle, title, sizeof(title));

                RECT rect;
                GetClientRect(handle, &rect);
                long area = (rect.right - rect.left) * (rect.bottom - rect.top);

                LOG_INFO("Found Window: Handle=%p, Class='%s', Title='%s', Area=%ld", handle, className, title, area);

                if (area > context->maxArea) {
                    context->maxArea = area;
                    context->bestHandle = handle;
                }

                return TRUE; // Continue enumerating to find the biggest one
            }

            HWND FindGameWindow()
            {
                WindowSearchContext context;
                EnumWindows(EnumWindowsCallback, (LPARAM)&context);
                Data::hWindow = context.bestHandle;
                return Data::hWindow;
            }
        }

        bool Init()
        {
            // CRITICAL FIX: Initialize MinHook explicitly before creating any input hooks.
            // Kiero initializes it too, but we need it earlier for input.
            if (MH_Initialize() != MH_OK && MH_Initialize() != MH_ERROR_ALREADY_INITIALIZED)
            {
                LOG_ERROR("Failed to initialize MinHook.");
                return false;
            }

            // Ensure settings exist
            if (!Data::pSettings) return false;

            // 1. Hook Inputs early
            // Now safe to call because MinHook is initialized.
            InitDirectInput();
            InitXInput();

            // 2. Initialize Kiero
            if (kiero::init(kiero::RenderType::Auto) != kiero::Status::Success)
            {
                LOG_ERROR("Kiero initialization failed.");
                return false;
            }

            kiero::RenderType::Enum type = kiero::getRenderType();
            LOG_INFO("Render Type: %s (%d)", RenderTypeToString(type), type);

            // 3. Find Window
            int attempts = 0;
            while (FindGameWindow() == NULL)
            {
                Sleep(100);
                if (++attempts > 100) { // 10 second timeout
                    LOG_ERROR("Timeout waiting for game window.");
                    return false;
                }
            }

            NotifyDirectInputWindow(Data::hWindow);

            // 4. Bind Graphics Hooks
            if (type == kiero::RenderType::D3D11) {
                kiero::bind(8, (void**)&Data::oPresent, hkPresentDX11);
                kiero::bind(13, (void**)&Data::oResizeBuffers, hkResizeBuffersDX11);
            }
            else if (type == kiero::RenderType::D3D10) {
                kiero::bind(8, (void**)&Data::oPresent, hkPresentDX10);
                kiero::bind(13, (void**)&Data::oResizeBuffers, hkResizeBuffersDX10);
            }
            else if (type == kiero::RenderType::D3D9) {
                kiero::bind(42, (void**)&Data::oEndScene, hkEndScene);
                kiero::bind(16, (void**)&Data::oReset, hkReset);
            }
            else {
                LOG_ERROR("Unsupported render type.");
                return false;
            }

            // 5. Hook WndProc last
            if (Data::hWindow) {
                 Data::oWndProc = (WndProc_t)SetWindowLongPtr(Data::hWindow, GWLP_WNDPROC, (LONG_PTR)Data::pSettings->m_WndProc);
                 LOG_INFO("WndProc Hooked. Original: %p", Data::oWndProc);
            }

            return true;
        }

        void RestoreWndProc()
        {
            if (Data::oWndProc && Data::hWindow)
            {
                SetWindowLongPtr(Data::hWindow, GWLP_WNDPROC, (LONG_PTR)Data::oWndProc);
                Data::oWndProc = nullptr; // Prevent double restore
                LOG_INFO("WndProc Restored.");
            }
        }

        void Shutdown()
        {
            // Ensure WndProc is restored if Shutdown called directly
            RestoreWndProc();

            if (ImGui::GetCurrentContext())
            {
                // Cleanup backend specific
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

            // Clean up internal input maps
            CleanupDirectInput();

            // Unhook Graphics
            kiero::shutdown();
        }
    }
}