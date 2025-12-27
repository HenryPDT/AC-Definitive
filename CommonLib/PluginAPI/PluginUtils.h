#pragma once

#include <Windows.h>
#include <filesystem>

namespace PluginUtils
{
    inline HMODULE ModuleFromAddress(const void* anyAddressInModule)
    {
        HMODULE hMod = nullptr;
        GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCSTR)anyAddressInModule,
            &hMod);
        return hMod;
    }

    inline std::filesystem::path ModulePath(HMODULE module)
    {
        char buf[MAX_PATH]{};
        GetModuleFileNameA(module, buf, MAX_PATH);
        return std::filesystem::path(buf);
    }

    inline std::filesystem::path ConfigRootDir(const void* anyAddressInModule)
    {
        auto p = ModulePath(ModuleFromAddress(anyAddressInModule));
        return p.parent_path() / "config";
    }
}


