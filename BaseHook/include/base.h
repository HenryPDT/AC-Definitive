#pragma once
#include <Windows.h>
#include <d3d9.h>
#include <d3d11.h>
#include <d3d10.h>
#include <dxgi.h>
#include <imgui.h>

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

    void Start(Settings& settings);
    bool Detach();
    void InitImGuiStyle(); // Helper to unify ImGui setup

    // To be implemented by user.
    void ImGuiLayer_WhenMenuIsOpen();
    void ImGuiLayer_EvenWhenMenuIsClosed();

    namespace Data
    {
        extern HMODULE           thisDLLModule;
        extern HWND              hWindow;
        extern bool              bShowMenu;
        extern bool              bIsInitialized;
        extern bool              bIsDetached;
        extern bool              bBlockInput;
        extern Settings*         pSettings;
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
    }

    class Settings
    {
    public:
        const WndProc_t m_WndProc;
    public:
        Settings(WndProc_t wndProc = BaseHook::Hooks::WndProc_Base)
            : m_WndProc(wndProc)
        {}

        virtual void OnActivate() = 0;
        virtual void OnDetach() = 0;

        virtual ~Settings() {}
    };
}