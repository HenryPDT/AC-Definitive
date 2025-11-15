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
            BOOL CALLBACK EnumWindowsCallback(HWND handle, LPARAM lParam)
            {
                DWORD wndProcId;
                GetWindowThreadProcessId(handle, &wndProcId);

                if (GetCurrentProcessId() != wndProcId || !IsWindowVisible(handle))
                    return TRUE;

                Data::hWindow = handle;
                return FALSE;
            }

            HWND FindGameWindow()
            {
                Data::hWindow = NULL;
                EnumWindows(EnumWindowsCallback, NULL);
                return Data::hWindow;
            }
        }

        bool Init()
        {
            kiero::RenderType::Enum forcedType = kiero::RenderType::None; // Set to None for auto-detection
            kiero::RenderType::Enum type;

            if (forcedType != kiero::RenderType::None)
            {
                // Use forced type for testing
                if (kiero::init(forcedType) != kiero::Status::Success)
                {
                    LOG_ERROR("Failed to initialize forced DirectX version");
                    return false;
                }
                type = forcedType;
            }
            else
            {
                // Use kiero's automatic detection to determine which DirectX version to hook
                if (kiero::init(kiero::RenderType::Auto) != kiero::Status::Success)
                {
                    LOG_ERROR("Kiero initialization failed - No supported D3D version found.");
                    return false;
                }
                type = kiero::getRenderType();
            }

            switch (type)
            {
            case kiero::RenderType::D3D11:
                LOG_INFO("Hooked D3D11");
                break;
            case kiero::RenderType::D3D10:
                LOG_INFO("Hooked D3D10");
                break;
            case kiero::RenderType::D3D9:
                LOG_INFO("Hooked D3D9");
                break;
            default:
                LOG_ERROR("Unsupported render type detected");
                return false;
            }

            // Hook DirectInput as early as possible, before the game window is even created.
            InitDirectInput();
            InitXInput();

            while (FindGameWindow() == NULL)
            {
                Sleep(100);
            }

            // Now that we have a window, we can hook the detected DirectX version and subclass the window.
            if (type == kiero::RenderType::D3D11) {
                kiero::bind(8, (void**)&Data::oPresent, hkPresentDX11);
                kiero::bind(13, (void**)&Data::oResizeBuffers, hkResizeBuffersDX11);
            }
            else if (type == kiero::RenderType::D3D10) {
                kiero::bind(8, (void**)&Data::oPresent, hkPresentDX10);
                kiero::bind(13, (void**)&Data::oResizeBuffers, hkResizeBuffersDX10);
            }
            else {
                // D3D9
                kiero::bind(42, (void**)&Data::oEndScene, hkEndScene);
                kiero::bind(16, (void**)&Data::oReset, hkReset);
            }

            Data::oWndProc = (WndProc_t)SetWindowLongPtr(Data::hWindow, GWLP_WNDPROC, (LONG_PTR)Data::pSettings->m_WndProc);
            return true;
        }

        void Shutdown()
        {
            if (Data::oWndProc)
                SetWindowLongPtr(Data::hWindow, GWLP_WNDPROC, (LONG_PTR)Data::oWndProc);

            // kiero::shutdown() will disable all hooks and uninitialize MinHook.
            // This includes the DirectInput hooks.
            kiero::shutdown();

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
        }
    }
}