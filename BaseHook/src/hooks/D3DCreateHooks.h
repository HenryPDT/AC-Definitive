#pragma once

namespace BaseHook::WindowedMode
{
    void InstallD3D9HooksIfNeeded(bool wantD3D9);
    void InstallD3D9HooksLate();
    void InstallDXGIHooksIfNeeded(bool wantDXGI);
    void InstallD3D10HooksIfNeeded(bool wantD3D10);
    void InstallD3D11HooksIfNeeded(bool wantD3D11);
}


