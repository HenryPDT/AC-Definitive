#pragma once
#include <Windows.h>
#include <d3d9.h>
#include <d3d11.h>
#include <d3d10.h>
#include <dxgi.h>
#include <imgui.h>
#include <atomic>

// DX9 Types
typedef long(__stdcall* EndScene_t)(LPDIRECT3DDEVICE9);
typedef HRESULT(__stdcall* Reset_t)(LPDIRECT3DDEVICE9, D3DPRESENT_PARAMETERS*);

// DX10/11 Types
typedef HRESULT(__stdcall* Present_t)(IDXGISwapChain*, UINT, UINT);
typedef HRESULT(__stdcall* ResizeBuffers_t)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);

typedef LRESULT(CALLBACK* WndProc_t)(HWND, UINT, WPARAM, LPARAM);

namespace BaseHook
{
    class Settings;

    void Start(Settings* settings); // Changed to pointer for explicit ownership semantics
    bool Detach();
    
    // Helper to access settings globally if needed
    Settings* GetSettings();

    void InitImGuiStyle();

    namespace Data
    {
        extern HMODULE           thisDLLModule;
        extern HWND              hWindow;
        extern Settings*         pSettings;
        extern bool              bShowMenu;
        extern bool              bIsInitialized;
        extern std::atomic<bool> bIsDetached; // Changed to atomic for thread safety
        extern bool              bBlockInput;
        extern std::atomic<bool> bIsRendering;
        extern WndProc_t         oWndProc;

        // DX9
        extern IDirect3DDevice9* pDevice;
        extern EndScene_t        oEndScene;
        extern Reset_t           oReset;

        // DXGI (Common)
        extern Present_t         oPresent;
        extern ResizeBuffers_t   oResizeBuffers;

        // DX11 Specific
        extern ID3D11Device*           pDevice11;
        extern ID3D11DeviceContext*    pContext11;
        extern ID3D11RenderTargetView* pMainRenderTargetView11;

        // DX10 Specific
        extern ID3D10Device*           pDevice10;
        extern ID3D10RenderTargetView* pMainRenderTargetView10;
    }

    namespace Keys
    {
        extern UINT ToggleMenu;
        extern UINT DetachDll;
    }

    namespace Hooks
    {
        LRESULT CALLBACK WndProc_Base(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

        // Hook declarations
        HRESULT __stdcall hkEndScene(LPDIRECT3DDEVICE9 pDevice);
        HRESULT __stdcall hkReset(LPDIRECT3DDEVICE9 pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters);
        HRESULT __stdcall hkPresentDX11(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
        HRESULT __stdcall hkResizeBuffersDX11(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags);
        HRESULT __stdcall hkPresentDX10(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
        HRESULT __stdcall hkResizeBuffersDX10(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags);

        void ApplyBufferedInput();
        void RestoreWndProc();
    }

    class Settings
    {
    public:
        WndProc_t m_WndProc;
        bool m_bSaveImGuiIni;

    public:
        Settings(WndProc_t wndProc = BaseHook::Hooks::WndProc_Base, bool bSaveImGuiIni = false)
            : m_WndProc(wndProc), m_bSaveImGuiIni(bSaveImGuiIni)
        {}

        // Lifecycle callbacks
        virtual void OnActivate() = 0;
        virtual void OnDetach() = 0;

        // Rendering callbacks (Implemented by user)
        virtual void DrawMenu() = 0;    // Replaces ImGuiLayer_WhenMenuIsOpen
        virtual void DrawOverlay() = 0; // Replaces ImGuiLayer_EvenWhenMenuIsClosed

        virtual ~Settings() {}
    };
}
