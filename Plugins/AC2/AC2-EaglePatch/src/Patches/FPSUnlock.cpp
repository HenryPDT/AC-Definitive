#include "FPSUnlock.h"
#include <AutoAssemblerKinda.h>
#include <PatternScanner.h>
#include <memory>
#include "log.h"

namespace AC2EaglePatch
{
    // Define the hook and its exits
    DEFINE_AOB_HOOK(FPSUnlock, "83 F8 10 73 10", 0, 5);
    DEFINE_EXITS(FPSUnlock,
        ContinueRender, 0x15,
        SleepCall, 0x0C
    );

    // Hook implementation
    HOOK_IMPL(FPSUnlock)
    {
        __asm {
            cmp eax, 1
            jb Label_Sleep
            jmp [FPSUnlock_ContinueRender]

        Label_Sleep:
            mov ecx, 1
            jmp [FPSUnlock_SleepCall]
        }
    }

    void InitFPSUnlock(uintptr_t baseAddr, GameVersion version, bool enable)
    {
        if (!FPSUnlock_Descriptor.IsResolved()) {
            return;
        }

        if (enable) {
            HookManager::Install(&FPSUnlock_Descriptor);
            LOG_INFO("[EaglePatch] FPS Unlock applied.");
        }
    }

    void SetFPSUnlock(bool enable)
    {
        if (enable) {
            HookManager::Install(&FPSUnlock_Descriptor);
        } else {
            HookManager::Uninstall(&FPSUnlock_Descriptor);
        }
    }
}