#include "EaglePatch.h"
#include "UplayBonus.h"
#include <AutoAssemblerKinda.h>
#include <PatternScanner.h>


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

    // Hook Wrapper
    DEFINE_HOOK(PlayerOptions_Hook, PlayerOptions_Return)
    {
        __asm {
            mov byte ptr [esi - 0x3D + 0x36], 1 // Bonus Dye
            mov byte ptr [esi - 0x3D + 0x3B], 1 // Knife Belt
            mov byte ptr [esi - 0x3D + 0x3C], 1 // Altair Robes
            mov byte ptr [esi - 0x3D + 0x3D], 1 // Auditore Crypt

            // Restore the original call
            // fnEndClass is __thiscall, ECX is already the thisPtr from the game
            call fnEndClass

            jmp [PlayerOptions_Return]
        }
    }

    struct UplayBonusHook : AutoAssemblerCodeHolder_Base
    {
        UplayBonusHook(uintptr_t hookAddr, uintptr_t endClassAddr)
        {
            fnEndClass = (EndClass_t)endClassAddr;
            PlayerOptions_Return = hookAddr + 5;
            PresetScript_InjectJump(hookAddr, (uintptr_t)&PlayerOptions_Hook);
        }
    };

    void InitUplayBonus(uintptr_t baseAddr, GameVersion version)
    {
        auto hookResult = Utils::PatternScanner::ScanMain("83 C6 3D 56 8B CF E8 ? ? ? ? 8B CF E8")
            .Offset(13);

        if (hookResult)
        {
            sAddresses::Hook = hookResult.As<uintptr_t>();
            
            // Extract the target of the 'call' instruction.
            // Assuming standard x86 relative call: E8 <4-byte-offset>
            // Instruction length is 5, offset is at index 1.
            sAddresses::EndClass = hookResult.ResolveRelative(5, 1).As<uintptr_t>();

            static AutoAssembleWrapper<UplayBonusHook> hook(sAddresses::Hook, sAddresses::EndClass);
            hook.Activate();

            if (g_loader_ref) g_loader_ref->LogToConsole("[EaglePatch] Uplay Bonuses enabled.");
        }
        else
        {
            if (g_loader_ref) g_loader_ref->LogToConsole("[EaglePatch] UplayBonus: Pattern NOT found!");
        }
    }
}
