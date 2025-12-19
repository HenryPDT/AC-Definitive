#include "pch.h"
#include "base.h"

namespace BaseHook
{
    namespace Hooks
    {
        void InitDirectInput();
        void InitXInput();

        // Forward declarations
        void InstallWndProcHook();
        void RestoreWndProc();

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

            // CreateWindowEx Hooks
            typedef HWND(WINAPI* CreateWindowExA_t)(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID);
            typedef HWND(WINAPI* CreateWindowExW_t)(DWORD, LPCWSTR, LPCWSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID);

            CreateWindowExA_t oCreateWindowExA = nullptr;
            CreateWindowExW_t oCreateWindowExW = nullptr;

            std::atomic<bool> g_WindowFound = false;
            long g_MaxArea = 0;

            HWND WINAPI hkCreateWindowExA(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam)
            {
                HWND hWnd = oCreateWindowExA(dwExStyle, lpClassName, lpWindowName, dwStyle, X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
                if (hWnd && !hWndParent) // Ignore child windows
                {
                    long area = nWidth * nHeight;
                    // If we haven't found a window yet, or this one is "better" (larger), take it.
                    if (!g_WindowFound || area > g_MaxArea)
                    {
                        if (g_WindowFound)
                        {
                            LOG_INFO("Switching to better window (Area: %ld > %ld): %p", area, g_MaxArea, hWnd);
                            RestoreWndProc(); // Unhook the previous one
                        }

                        LOG_INFO("Caught CreateWindowExA: %p (Area: %ld)", hWnd, area);
                        Data::hWindow = hWnd;
                        g_MaxArea = area;
                        g_WindowFound = true;
                        InstallWndProcHook();
                    }
                }
                return hWnd;
            }

            HWND WINAPI hkCreateWindowExW(DWORD dwExStyle, LPCWSTR lpClassName, LPCWSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam)
            {
                HWND hWnd = oCreateWindowExW(dwExStyle, lpClassName, lpWindowName, dwStyle, X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
                if (hWnd && !hWndParent)
                {
                    long area = nWidth * nHeight;
                    if (!g_WindowFound || area > g_MaxArea)
                    {
                        if (g_WindowFound)
                        {
                            LOG_INFO("Switching to better window (Area: %ld > %ld): %p", area, g_MaxArea, hWnd);
                            RestoreWndProc();
                        }

                        LOG_INFO("Caught CreateWindowExW: %p (Area: %ld)", hWnd, area);
                        Data::hWindow = hWnd;
                        g_MaxArea = area;
                        g_WindowFound = true;
                        InstallWndProcHook();
                    }
                }
                return hWnd;
            }
        }

        void InstallWndProcHook()
        {
            if (!Data::hWindow) return;
            
            // Just hook WndProc. Defer graphics init to FinishInitialization().
            if (Data::oWndProc == nullptr) {
                 Data::oWndProc = (WndProc_t)SetWindowLongPtr(Data::hWindow, GWLP_WNDPROC, (LONG_PTR)Data::pSettings->m_WndProc);
                 LOG_INFO("WndProc Hooked via CreateWindow. Original: %p", Data::oWndProc);
            }

            // Always notify DirectInput about the current window (important for window switching)
            NotifyDirectInputWindow(Data::hWindow);

            // Force initialization immediately. 
            FinishInitialization();
        }

        void FinishInitialization()
        {
            if (Data::bGraphicsInitialized) return;
            Data::bGraphicsInitialized = true; // Set flag immediately to prevent re-entry

            LOG_INFO("Finishing Initialization for Window: %p", Data::hWindow);

            // Bind Graphics Hooks
            kiero::RenderType::Enum type = kiero::getRenderType();
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
                LOG_ERROR("Unsupported render type in FinishInitialization.");
            }

            // Disable CreateWindow hooks as we are done
            if (oCreateWindowExA) MH_DisableHook((LPVOID)GetProcAddress(GetModuleHandleA("user32.dll"), "CreateWindowExA"));
            if (oCreateWindowExW) MH_DisableHook((LPVOID)GetProcAddress(GetModuleHandleA("user32.dll"), "CreateWindowExW"));
        }

        bool Init()
        {
            // CRITICAL FIX: Initialize MinHook explicitly before creating any input hooks.
            if (MH_Initialize() != MH_OK && MH_Initialize() != MH_ERROR_ALREADY_INITIALIZED)
            {
                LOG_ERROR("Failed to initialize MinHook.");
                return false;
            }

            // Ensure settings exist
            if (!Data::pSettings) return false;

            // 1. Hook Inputs early
            InitDirectInput();
            InitXInput();

            // 2. Initialize Kiero (creates dummy device to find addresses)
            if (kiero::init(kiero::RenderType::Auto) != kiero::Status::Success)
            {
                LOG_ERROR("Kiero initialization failed.");
                return false;
            }

            kiero::RenderType::Enum type = kiero::getRenderType();
            LOG_INFO("Render Type: %s (%d)", RenderTypeToString(type), type);

            // 3. Check if Window already exists (Late Injection)
            if (FindGameWindow())
            {
                LOG_INFO("Window already exists, hooking immediately.");
                InstallWndProcHook();
                return true;
            }

            // 4. Hook CreateWindowEx (Early Injection)
            LOG_INFO("Window not found, hooking CreateWindowEx.");
            
            MH_CreateHookApi(L"user32.dll", "CreateWindowExA", hkCreateWindowExA, (LPVOID*)&oCreateWindowExA);
            MH_CreateHookApi(L"user32.dll", "CreateWindowExW", hkCreateWindowExW, (LPVOID*)&oCreateWindowExW);
            
            MH_EnableHook((LPVOID)GetProcAddress(GetModuleHandleA("user32.dll"), "CreateWindowExA"));
            MH_EnableHook((LPVOID)GetProcAddress(GetModuleHandleA("user32.dll"), "CreateWindowExW"));

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