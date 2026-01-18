#pragma once
#include <cstdint>
#include <Xinput.h>
#include "../EaglePatch.h"
#include "Scimitar/Pad.h"
#include "Gear/Memory.h"

namespace AC2EaglePatch
{
    void InitController(uintptr_t baseAddr, GameVersion version, bool enable, int keyboardLayout);
    void UpdateKeyboardLayout(int keyboardLayout);

    // --- Game Structures (scimitar) ---

    namespace scimitar
    {
        using InputBindings = AC2::Scimitar::InputBindings;

        // Alias AC2-RE types to local namespace for compatibility with existing cpp code
        using Pad = AC2::Scimitar::Pad;
        using PadXenon = AC2::Scimitar::PadXenon;
        using PadProxyPC = AC2::Scimitar::PadProxyPC;
        using PadSets = AC2::Scimitar::PadSets;
        using PadData = AC2::Scimitar::PadData;
    }

    namespace Gear
    {
        using MemHook = AC2::Gear::MemHook;
    }

    // Function pointer typedefs
    using t_ac_getNewDescriptor = void*(__cdecl*)(uint32_t, uint32_t, uint32_t);
    using t_ac_getDeleteDescriptor = uint32_t(__thiscall*)(void*, void*);
    using t_ac_allocate = void*(__cdecl*)(int, uint32_t, void*, const void*, const char*, const char*, uint32_t, const char*);
    using t_ac_delete = void(__cdecl*)(void*, void*, const char*);
}