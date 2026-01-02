#pragma once

#include <Windows.h>
#include <d3d9.h>
#include <dxgi.h>

namespace BaseHook
{
    namespace Hooks
    {
        void InstallD3D9CreateHooks();
        void InstallDXGICreateHooks();
        void InstallD3D10CreateHooks();
        void InstallD3D11CreateHooks();
        void InstallD3D9CreateHooksLate();
        void InstallDXGIHooksLate();
    }
}