#pragma once

namespace BaseHook::WindowedMode
{
    void InstallD3D9HooksIfNeeded(bool wantD3D9);
    void InstallDXGIHooksIfNeeded(bool wantDXGI);
    void InstallD3D10HooksIfNeeded(bool wantD3D10);
}


