#include "EaglePatch.h"
#include "UplayBonus.h"
#include <AutoAssemblerKinda.h>
#include "log.h"

namespace AC2EaglePatch
{
    // ==========================================================================
    // Uplay Bonus Items Unlock
    // ==========================================================================
    // Hooks the PlayerOptions destructor to enable all Uplay bonuses before cleanup.
    // 
    // Pattern: 83 C6 3D 56 8B CF E8 ? ? ? ? 8B CF E8
    //          ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ Hook site (offset +13 = the E8 call)
    //                                         The E8 CALL is to EndClass function
    //
    // We hook at the call site, set the bonus flags, then call the original function.
    // ==========================================================================

    // Function pointer extracted from the pattern (the CALL target)
    using EndClass_t = void(__thiscall*)(void* thisPtr);
    static EndClass_t fnEndClass = nullptr;

    // The actual hook (defined first so the address can derive from it)
    DEFINE_AOB_HOOK(UplayBonus, "83 C6 3D 56 8B CF E8 ? ? ? ? 8B CF E8", 13, 5);

    // Derive from UplayBonus hook (offset 0 since hook is already at +13, then resolve CALL)
    DEFINE_ADDRESS(UplayEndClass, "@UplayBonus", 0, CALL, (uintptr_t*)&fnEndClass);

    HOOK_IMPL(UplayBonus)
    {
        __asm {
            // ESI points to PlayerOptions + 0x3D at this point
            // Set all Uplay bonus flags
            mov byte ptr [esi - 0x3D + 0x36], 1  // Bonus Dye
            mov byte ptr [esi - 0x3D + 0x3B], 1  // Knife Belt
            mov byte ptr [esi - 0x3D + 0x3C], 1  // Altair Robes
            mov byte ptr [esi - 0x3D + 0x3D], 1  // Auditore Crypt

            // Call the original EndClass function
            // ECX is already set to thisPtr by the game
            call fnEndClass

            jmp [UplayBonus_Return]
        }
    }

    void InitUplayBonus(uintptr_t baseAddr, GameVersion version, bool enable)
    {
        // First resolve the function pointer
        if (!UplayEndClass_Desc.IsResolved()) {
            return;
        }

        // Then resolve and install the hook
        if (!UplayBonus_Descriptor.IsResolved()) {
            return;
        }

        if (enable)
        {
            HookManager::Install(&UplayBonus_Descriptor);
            LOG_INFO("[EaglePatch] Uplay Bonuses enabled.");
        }
    }
}
