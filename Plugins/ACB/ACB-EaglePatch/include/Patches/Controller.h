#pragma once
#include <cstdint>
#include <Xinput.h>
#include "../EaglePatch.h"
#include "Scimitar/Pad.h"
#include "Gear/Memory.h"

namespace ACBEaglePatch
{
    void InitController(uintptr_t baseAddr, GameVersion version, int keyboardLayout);
    void UpdateKeyboardLayout(int keyboardLayout);

    // --- Game Structures (scimitar) ---
    namespace scimitar
    {
        using InputBindings = ACB::Scimitar::InputBindings;

        using Pad = ACB::Scimitar::Pad;
        using PadXenon = ACB::Scimitar::PadXenon;
        using PadProxyPC = ACB::Scimitar::PadProxyPC;
        using PadSets = ACB::Scimitar::PadSets;
        using PadData = ACB::Scimitar::PadData;
    }

    // --- Engine Allocator Interfaces ---
    namespace Gear
    {
        using MemHook = ACB::Gear::MemHook;
    }


    // Function pointer typedefs
    using t_ac_getNewDescriptor = void*(__cdecl*)(uint32_t, uint32_t, uint32_t);
    using t_ac_getDeleteDescriptor = uint32_t(__thiscall*)(void*, void*);
    using t_ac_allocate = void*(__cdecl*)(int, uint32_t, void*, const void*, const char*, const char*, uint32_t, const char*);
    using t_ac_delete = void(__cdecl*)(void*, void*, const char*);
}