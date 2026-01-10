#pragma once
#include <Windows.h>
#include <d3d9.h>
#include <d3d11.h>
#include <d3d10.h>
#include <dxgi.h>
#include <xinput.h>
#include <imgui.h>
#include <atomic>
#include <cstdint>
#include "hooks/Hooks.h"

// DX9 Types
typedef long(__stdcall* EndScene_t)(LPDIRECT3DDEVICE9);
typedef HRESULT(__stdcall* Reset_t)(LPDIRECT3DDEVICE9, D3DPRESENT_PARAMETERS*);
typedef HRESULT(__stdcall* ResetEx_t)(IDirect3DDevice9Ex*, D3DPRESENT_PARAMETERS*, D3DDISPLAYMODEEX*);
typedef HRESULT(__stdcall* Present9_t)(LPDIRECT3DDEVICE9, CONST RECT*, CONST RECT*, HWND, CONST RGNDATA*);
typedef HRESULT(__stdcall* TestCooperativeLevel_t)(LPDIRECT3DDEVICE9);

// DX10/11 Types
typedef HRESULT(__stdcall* Present_t)(IDXGISwapChain*, UINT, UINT);
typedef HRESULT(__stdcall* ResizeBuffers_t)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
typedef HRESULT(__stdcall* ResizeTarget_t)(IDXGISwapChain*, const DXGI_MODE_DESC*);

// User32 Types
typedef int(WINAPI* GetSystemMetrics_t)(int);
typedef BOOL(WINAPI* AdjustWindowRectEx_t)(LPRECT, DWORD, BOOL, DWORD);
typedef BOOL(WINAPI* GetClientRect_t)(HWND, LPRECT);
typedef BOOL(WINAPI* GetWindowRect_t)(HWND, LPRECT);
typedef BOOL(WINAPI* ClientToScreen_t)(HWND, LPPOINT);
typedef BOOL(WINAPI* ScreenToClient_t)(HWND, LPPOINT);
typedef HWND(WINAPI* WindowFromPoint_t)(POINT);

typedef LRESULT(CALLBACK* WndProc_t)(HWND, UINT, WPARAM, LPARAM);

namespace BaseHook
{
    class Settings;

    enum class FakeResetState : int
    {
        Clear = 0,
        Initiate = 1,
        Respond = 2
    };

    enum class DirectXVersion : int
    {
        Auto = 0,
        DX9 = 9,
        DX10 = 10,
        DX11 = 11
    };

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
        extern bool              bShowConsole;
        extern bool              bIsInitialized;
        extern std::atomic<bool> bIsDetached; // Changed to atomic for thread safety
        extern bool              bBlockInput;
        extern std::atomic<bool> bIsRendering;
        extern bool              bGraphicsInitialized;
        extern bool              bFixDirectInput; // Moved here for global visibility
        // Controls how overlay (ImGui) mouse buttons/wheel are fed:
        // - false: Win32 messages feed ImGui mouse buttons/wheel
        // - true:  DirectInput feeds ImGui mouse buttons/wheel (via ApplyBufferedInput)
        // Overlay mouse movement + keyboard remain Win32/WndProc always.
        extern bool              bImGuiMouseButtonsFromDirectInput;
        extern WndProc_t         oWndProc;

        // Original User32 Pointers (for internal use to avoid hook recursion)
        extern FARPROC oGetSystemMetrics;
        extern FARPROC oAdjustWindowRectEx;
        extern FARPROC oGetClientRect;
        extern FARPROC oGetWindowRect;
        extern FARPROC oClientToScreen;
        extern FARPROC oScreenToClient;
        extern FARPROC oGetCursorPos;
        extern FARPROC oWindowFromPoint;

        // Input Management
        extern thread_local bool                   bCallingImGui;   // True when ImGui_ImplWin32_NewFrame is polling inputs
        extern std::atomic<unsigned long long>     lastXInputTime;  // Timestamp of last successful XInput poll

        // DX9
        extern IDirect3DDevice9* pDevice;
        extern EndScene_t        oEndScene;
        extern Reset_t           oReset;
        extern ResetEx_t         oResetEx;
        extern Present9_t        oPresent9;
        extern TestCooperativeLevel_t oTestCooperativeLevel;
        extern std::atomic<FakeResetState> g_fakeResetState;

        // DXGI (Common)
        extern IDXGISwapChain*   pSwapChain;
        extern Present_t         oPresent;
        extern ResizeBuffers_t   oResizeBuffers;
        extern ResizeTarget_t    oResizeTarget;

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
        extern UINT DetachDll;
    }

    class Settings
    {
    public:
        WndProc_t m_WndProc;
        bool m_bSaveImGuiIni;

        // Windowed Mode Settings
        int m_WindowedMode; // 0=Off, 1=Fullscreen, 2=Borderless, 3=Bordered
        int m_ResizeBehavior; // 0=Fixed, 1=MatchGame, 2=Scale
        int m_WindowPosX;
        int m_WindowPosY;
        int m_WindowWidth;
        int m_WindowHeight;
        int m_CursorClipMode; // 0=Default, 1=Confine, 2=Unlock

        // DirectX Version
        int m_DirectXVersion; // 0=Auto, 9, 10, 11

        bool m_RenderInBackground;

    public:
        Settings(WndProc_t wndProc = BaseHook::Hooks::WndProc_Base, bool bSaveImGuiIni = false)
            : m_WndProc(wndProc), m_bSaveImGuiIni(bSaveImGuiIni),
              m_WindowedMode(0), m_ResizeBehavior(0), m_WindowPosX(-1), m_WindowPosY(-1), m_WindowWidth(1920), m_WindowHeight(1080),
              m_CursorClipMode(0), m_DirectXVersion(0), m_RenderInBackground(false)
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
