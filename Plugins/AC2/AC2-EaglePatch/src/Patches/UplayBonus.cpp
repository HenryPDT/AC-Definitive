#include "../EaglePatch.h"
#include "UplayBonus.h"
#include <AutoAssemblerKinda.h>

namespace AC2EaglePatch
{
    struct sAddresses {
        static uintptr_t Hook;
        static uintptr_t EndClass;
    };

    uintptr_t sAddresses::Hook = 0;
    uintptr_t sAddresses::EndClass = 0;

    typedef void(__thiscall* EndClass_t)(void* thisPtr);
    static EndClass_t fnEndClass = nullptr;

    namespace
    {
        bool ResolveAddresses(uintptr_t baseAddr, GameVersion version)
        {
            switch (version)
            {
            case GameVersion::Version1: // Uplay
                sAddresses::Hook = baseAddr + 0x6D4723;
                sAddresses::EndClass = baseAddr + 0x10BEB50;
                return true;
            case GameVersion::Version2: // Retail 1.01
                sAddresses::Hook = baseAddr + 0xCB06B3;
                sAddresses::EndClass = baseAddr + 0x5FB540;
                return true;
            default:
                return false;
            }
        }
    }

    void Hook_PlayerOptions(AllRegisters* params)
    {
        // The game logic here calculates an offset from ESI.
        // We adjust ESI temporarily to access the UPlay unlock flags.
        
        uint8_t* data = (uint8_t*)(params->esi - 0x3D);

        // Enable UPlay bonuses
        data[0x36] = 1; // Bonus Dye
        data[0x3B] = 1; // Knife Belt
        data[0x3C] = 1; // Altair Robes
        data[0x3D] = 1; // Auditore Crypt

        // Call EndClass(ECX) - Restoring the original function call we overwrote
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
            
            // Hook at the call site of EndClass.
            // We execute our unlocking logic, manually call EndClass, and then return to the instruction after the hook.
            // executeStolenBytes = false because we handle the call manually.
            
            PresetScript_CCodeInTheMiddle(hookAddr, 5, Hook_PlayerOptions, std::nullopt, false);
        }
    };

    void InitUplayBonus(uintptr_t baseAddr, GameVersion version)
    {
        if (!ResolveAddresses(baseAddr, version))
            return;

        static AutoAssembleWrapper<UplayBonusHook> hook(sAddresses::Hook, sAddresses::EndClass);
        hook.Activate();

        if (g_loader_ref) g_loader_ref->LogToConsole("[EaglePatch] Uplay Bonuses enabled.");
    }
}