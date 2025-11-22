#include "pch.h"
#include "base.h"
#include <filesystem>

namespace BaseHook
{
    namespace Data
    {
        HMODULE           thisDLLModule = NULL;
        HWND              hWindow = NULL;
        bool              bShowMenu = false;
        bool              bIsInitialized = false;
        std::atomic<bool> bIsDetached = false;
        bool              bBlockInput = false;
        std::atomic<bool> bIsRendering = false;
        bool              bFixDirectInput = true;
        Settings*         pSettings = nullptr;
        WndProc_t         oWndProc = nullptr;

        thread_local bool               bCallingImGui = false;
        std::atomic<unsigned long long> lastXInputTime = 0;

        // DX9
        IDirect3DDevice9* pDevice = NULL;
        EndScene_t        oEndScene = nullptr;
        Reset_t           oReset = nullptr;

        // DX11/10 (DXGI)
        Present_t         oPresent = nullptr;
        ResizeBuffers_t   oResizeBuffers = nullptr;
        ID3D11Device*           pDevice11 = nullptr;
        ID3D11DeviceContext*    pContext11 = nullptr;
        ID3D11RenderTargetView* pMainRenderTargetView11 = nullptr;

        // DX10
        ID3D10Device*           pDevice10 = nullptr;
        ID3D10RenderTargetView* pMainRenderTargetView10 = nullptr;
    }

    namespace Keys
    {
        UINT ToggleMenu = VK_INSERT;
        UINT DetachDll = VK_END;
    }

    namespace Hooks
    {
        bool Init();
        void Shutdown();
        void RestoreWndProc(); // Helper to restore WndProc separately
    }

    void Start(Settings* settings)
    {
        if (!settings) return;
        Data::pSettings = settings;
        Data::pSettings->OnActivate();
        Hooks::Init();
    }

    Settings* GetSettings() {
        return Data::pSettings;
    }

    bool Detach()
    {
        if (Data::bIsDetached) return true;
        
        LOG_INFO("Detaching...");
        Data::bIsDetached = true;
        
        // 1. Notify user code
        if (Data::pSettings)
            Data::pSettings->OnDetach();

        // 2a. Yield to ensure Render threads see the Detach flag
        // Prevents race where Render thread passes check but hasn't set bIsRendering yet
        Sleep(50);

        // 2. Wait for rendering to finish current frame
        // Timeout after 500ms to prevent infinite freeze if render thread dies
        int timeout = 0;
        while (Data::bIsRendering && timeout < 500) { 
            Sleep(1); 
            timeout++;
        }

        // 3. Restore WndProc first to stop processing input events
        Hooks::RestoreWndProc();

        // 4. Full Shutdown (Removes hooks, unloads ImGui)
        Hooks::Shutdown();
        
        Data::bIsInitialized = false;
        return true;
    }

    void LoadSystemFonts()
    {
        ImGuiIO& io = ImGui::GetIO();
        char windir[MAX_PATH];
        if (GetWindowsDirectoryA(windir, MAX_PATH))
        {
            std::filesystem::path fontsDir = std::filesystem::path(windir) / "Fonts";
            const char* fontNames[] = { "CascadiaCode.ttf", "consola.ttf", "segoeui.ttf", "arial.ttf" };

            for (const char* fontName : fontNames)
            {
                std::filesystem::path fontPath = fontsDir / fontName;
                if (std::filesystem::exists(fontPath))
                {
                    io.Fonts->Clear();
                    ImFontConfig config;
                    io.Fonts->AddFontFromFileTTF(fontPath.string().c_str(), 16.0f, &config);
                    LOG_INFO("Loaded system font: %s", fontName);
                    return; 
                }
            }
        }
        
        // Only add default if nothing else loaded
        if (io.Fonts->Fonts.empty())
            io.Fonts->AddFontDefault();
    }

    void InitImGuiStyle()
    {
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        
        // Use settings for IniFilename
        io.IniFilename = (Data::pSettings && Data::pSettings->m_bSaveImGuiIni) ? "imgui.ini" : NULL;
        
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
        io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
        LoadSystemFonts();
        
        ImGui::StyleColorsDark();
    }
}
