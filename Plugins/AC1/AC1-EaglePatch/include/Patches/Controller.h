#pragma once
#include <cstdint>
#include <Xinput.h>
#include "../EaglePatch.h"
#include "Scimitar/Pad.h"

namespace AC1EaglePatch
{
    void InitController(uintptr_t baseAddr, GameVersion version, int keyboardLayout);
    void UpdateKeyboardLayout(int keyboardLayout);

    // --- Game Structures (scimitar) ---
    namespace scimitar
    {
        using InputBindings = AC1::Scimitar::InputBindings;

        using Pad = AC1::Scimitar::Pad;
        using PadXenon = AC1::Scimitar::PadXenon;
        using PadProxyPC = AC1::Scimitar::PadProxyPC;
        using PadSets = AC1::Scimitar::PadSets;
        using PadData = AC1::Scimitar::PadData;
    }

    // Function pointer typedefs
    using t_ac_getNewDescriptor = void* (__cdecl*)(uint32_t, uint32_t, uint32_t);
    using t_ac_allocate = void* (__cdecl*)(int, uint32_t, void*, const void*, const char*, const char*, uint32_t, const char*);
    using t_ac_delete = void(__cdecl*)(void*, void*, const char*);
}