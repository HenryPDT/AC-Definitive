#pragma once
#include <IPlugin.h>
#include <cstdint>
#include <windows.h>

// Global reference to the loader interface
extern const PluginLoaderInterface* g_loader_ref;

namespace AC2EaglePatch
{
    enum class GameVersion
    {
        Unknown,
        Version1, // Uplay build: marker at +0x92BA56 == 0xFF8968E6
        Version2  // Retail 1.01: marker at +0xF06056 == 0xFF2E4F96
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
        if (safeRead(baseAddr + 0x92BA56, v1) && v1 == 0xFF8968E6)
            return GameVersion::Version1;
        if (safeRead(baseAddr + 0xF06056, v2) && v2 == 0xFF2E4F96)
            return GameVersion::Version2;
        return GameVersion::Unknown;
    }
}