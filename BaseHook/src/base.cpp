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
        bool              bIsDetached = false;
        bool              bBlockInput = false;
        Settings*         pSettings = nullptr;
        WndProc_t         oWndProc = nullptr;

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
    }

    void Start(Settings& settings)
    {
        Data::pSettings = &settings;
        settings.OnActivate();
        Hooks::Init();
    }

    bool Detach()
    {
        Data::bIsDetached = true;
        Data::pSettings->OnDetach();
        Sleep(100); // Allow render thread to finish current frame before destroying hooks/context
        Hooks::Shutdown();
        Data::bIsInitialized = false;
        return true;
    }

    void LoadSystemFonts()
    {
        ImGuiIO& io = ImGui::GetIO();
        // Try to find a nice monospaced font
        char windir[MAX_PATH];
        if (GetWindowsDirectoryA(windir, MAX_PATH))
        {
            std::filesystem::path fontsDir = std::filesystem::path(windir) / "Fonts";
            // Preference list: Cascadia Code (Win10/11), Consolas, Segoe UI
            const char* fontNames[] = { "CascadiaCode.ttf", "consola.ttf", "segoeui.ttf" };

            for (const char* fontName : fontNames)
            {
                std::filesystem::path fontPath = fontsDir / fontName;
                if (std::filesystem::exists(fontPath))
                {
                    io.Fonts->Clear();
                    ImFontConfig config;
                    // slightly larger than default 13px for readability
                    io.Fonts->AddFontFromFileTTF(fontPath.string().c_str(), 16.0f, &config);
                    LOG_INFO("Loaded system font: %s", fontName);
                    break;
                }
            }
        }

        // Fallback if no fonts found or load failed
        if (io.Fonts->Fonts.empty())
            io.Fonts->AddFontDefault();
    }

    void InitImGuiStyle()
    {
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = NULL;
        io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
        LoadSystemFonts();
    }
}