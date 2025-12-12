#pragma once
#include <IPlugin.h>
#include <cstdint>
#include <windows.h>

// Global reference to the loader interface
extern const PluginLoaderInterface* g_loader_ref;

namespace ACREaglePatch
{
    enum class GameVersion
    {
        Unknown,
        Version1, // Marker at +0x0BBC866 == 0xFF602516
        Version2  // Marker at +0x0BF0306 == 0xFF5E9006
    };

    inline GameVersion DetectVersion(uintptr_t baseAddr)
    {
        auto safeRead = [](uintptr_t addr, uint32_t& out) -> bool
        {
            if (IsBadReadPtr((void*)addr, sizeof(uint32_t))) return false;
            out = *(uint32_t*)addr;
            return true;
        };

        uint32_t v1 = 0, v2 = 0;
        if (safeRead(baseAddr + 0x0BBC866, v1) && v1 == 0xFF602516)
            return GameVersion::Version1;
        if (safeRead(baseAddr + 0x0BF0306, v2) && v2 == 0xFF5E9006)
            return GameVersion::Version2;
        return GameVersion::Unknown;
    }
}