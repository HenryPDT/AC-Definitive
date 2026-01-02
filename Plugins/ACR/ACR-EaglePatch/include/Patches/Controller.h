#pragma once
#include <cstdint>
#include <Xinput.h>
#include "../EaglePatch.h"
#include "Scimitar/Pad.h"
#include "Gear/Memory.h"

namespace ACREaglePatch
{
    void InitController(uintptr_t baseAddr, GameVersion version, int keyboardLayout);
    void UpdateKeyboardLayout(int keyboardLayout);

    // --- Game Structures (scimitar) ---
    namespace scimitar
    {
        using InputBindings = ACR::Scimitar::InputBindings;

        using Pad = ACR::Scimitar::Pad;
        using PadXenon = ACR::Scimitar::PadXenon;
        using PadProxyPC = ACR::Scimitar::PadProxyPC;
        using PadSets = ACR::Scimitar::PadSets;
        using PadData = ACR::Scimitar::PadData;
    }

    // --- Engine Allocator Interfaces ---
    namespace Gear
    {
        using MemHook = ACR::Gear::MemHook;
    }


    // Function pointer typedefs
    using t_ac_getNewDescriptor = void*(__cdecl*)(uint32_t, uint32_t, uint32_t);
    using t_ac_getDeleteDescriptor = uint32_t(__thiscall*)(void*, void*);
    using t_ac_allocate = void*(__cdecl*)(int, uint32_t, void*, const void*, const char*, const char*, uint32_t, const char*);
    using t_ac_delete = void(__cdecl*)(void*, void*, const char*);
}