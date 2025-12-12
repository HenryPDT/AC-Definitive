#pragma once
#include <IPlugin.h>
#include <cstdint>
#include <windows.h>

// Global reference to the loader interface
extern const PluginLoaderInterface* g_loader_ref;

namespace AC1EaglePatch
{
    enum class GameVersion
    {
        Unknown,
        Version1, // DX10 build: marker 0xFFA5C438 @ +0x8F6F34
        Version2  // DX9 build:  marker 0xFFBF81A8 @ +0x720244
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
        // DX10 marker (Version1)
        if (safeRead(baseAddr + 0x8F6F34, v1) && v1 == 0xFFA5C438)
            return GameVersion::Version1;
        // DX9 marker (Version2)
        if (safeRead(baseAddr + 0x720244, v2) && v2 == 0xFFBF81A8)
            return GameVersion::Version2;

        return GameVersion::Unknown;
    }
}