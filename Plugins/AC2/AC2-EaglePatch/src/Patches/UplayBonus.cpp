#include "../EaglePatch.h"
#include "UplayBonus.h"
#include <AutoAssemblerKinda.h>

namespace AC2EaglePatch
{
    typedef void(__thiscall* EndClass_t)(void* thisPtr);
    static EndClass_t fnEndClass = nullptr;

    void Hook_PlayerOptions(AllRegisters* params)
    {
        // sub esi, 0x3D
        // In the original manual patch, it did:
        // sub esi, 0x3D
        // ... writes ...
        // add esi, 0x3D
        // call EndClass
        
        // ESI seems to be the pointer to the data structure (or offset from it).
        // Let's assume ESI is the pointer we want to modify relative to - 0x3D.
        
        uint8_t* data = (uint8_t*)(params->esi - 0x3D);

        // Enable UPlay bonuses
        data[0x36] = 1; // Bonus Dye
        data[0x3B] = 1; // Knife Belt
        data[0x3C] = 1; // Altair Robes
        data[0x3D] = 1; // Auditore Crypt

        // Call EndClass(ECX)
        // Original code: call EndClass
        // We need to call it manually.
        if (fnEndClass)
        {
            fnEndClass((void*)params->ecx);
        }
    }

    struct UplayBonusHook : AutoAssemblerCodeHolder_Base
    {
        UplayBonusHook(uintptr_t hookAddr, uintptr_t endClassAddr)
        {
            fnEndClass = (EndClass_t)endClassAddr;
            
            // Replaces the call to EndClass or whatever was there.
            // Original code:
            // call EndClass
            // retn
            
            // We hook at `hookAddr`. 
            // `PresetScript_CCodeInTheMiddle` will replace 5 bytes.
            // We pass `false` for `executeStolenBytes` because `Hook_PlayerOptions` does the work (including calling EndClass).
            // `whereToReturn` is implicitly next instruction if we don't specify or if we return normally.
            // But wait, `Hook_PlayerOptions` calls `EndClass`.
            // Does it need to RETN?
            // The hook wrapper restores registers and then jumps back to `whereToReturn`.
            // If `whereToReturn` is `std::nullopt`, it jumps to `hookAddr + 5`.
            // If the original instruction was a CALL (5 bytes) followed by RETN.
            // Then jumping to `hookAddr + 5` (which is RETN) is correct.
            
            PresetScript_CCodeInTheMiddle(hookAddr, 5, Hook_PlayerOptions, std::nullopt, false);
        }
    };

    void InitUplayBonus(uintptr_t baseAddr, GameVersion version)
    {
        uintptr_t hookAddr = 0;
        uintptr_t endClassAddr = 0;

        switch (version)
        {
        case GameVersion::Version1: // Uplay
            hookAddr = baseAddr + 0x6D4723;
            endClassAddr = baseAddr + 0x10BEB50;
            break;

        case GameVersion::Version2: // Retail 1.01
            hookAddr = baseAddr + 0xCB06B3;
            endClassAddr = baseAddr + 0x5FB540;
            break;

        default: return;
        }

        static AutoAssembleWrapper<UplayBonusHook> hook(hookAddr, endClassAddr);
        hook.Activate();

        if (g_loader_ref) g_loader_ref->LogToConsole("[EaglePatch] Uplay Bonuses enabled.");
    }
}