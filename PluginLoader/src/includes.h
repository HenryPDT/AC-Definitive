#pragma once
#include <Windows.h>
#include <string>
#include <filesystem>
#include <d3d9.h>
#include <d3dx9.h>
#include <DbgHelp.h>
#include "../../CommonLib/Kiero/kiero.h"
#include "../../CommonLib/Kiero/minhook/include/MinHook.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx9.h"
#include "../../CommonLib/Common_PluginSide/public_headers/IPlugin.h"
#include "../../CommonLib/Utils/include/log.h"
#include "../../CommonLib/Utils/include/crash_handler.h"

#pragma comment(lib, "dbghelp.lib")

typedef long(__stdcall* EndScene)(LPDIRECT3DDEVICE9);
typedef HRESULT(__stdcall* Reset)(LPDIRECT3DDEVICE9, D3DPRESENT_PARAMETERS*);
typedef LRESULT(CALLBACK* WNDPROC)(HWND, UINT, WPARAM, LPARAM);

