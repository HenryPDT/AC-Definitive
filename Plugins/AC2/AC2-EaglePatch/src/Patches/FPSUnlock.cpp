#include "FPSUnlock.h"
#include <AutoAssemblerKinda.h>
#include <PatternScanner.h>
#include <memory>
#include "log.h"

namespace AC2EaglePatch
{
    static uintptr_t s_ContinueRender = 0;
    static uintptr_t s_SleepCall = 0;

    DEFINE_HOOK(FPSUnlock_Hook, FPSUnlock_Return)
    {
        __asm {
            cmp eax, 1
            jb Label_Sleep
            jmp [s_ContinueRender]

        Label_Sleep:
            mov ecx, 1
            jmp [s_SleepCall]
        }
    }

    struct FPSUnlockPatch : AutoAssemblerCodeHolder_Base
    {
        FPSUnlockPatch(uintptr_t addr) {
            PresetScript_InjectJump(addr, (uintptr_t)&FPSUnlock_Hook);
        }
    };

    using FPSUnlockWrapper = AutoAssembleWrapper<FPSUnlockPatch>;
    static std::unique_ptr<FPSUnlockWrapper> s_FPSUnlockPatch;

    void InitFPSUnlock(uintptr_t baseAddr, GameVersion version, bool enable)
    {
        auto pattern = Utils::PatternScanner::ScanModule("AssassinsCreedIIGame.exe", "83 F8 10 73 10");
        if (!pattern) {
             LOG_INFO("[AC2 EaglePatch] FPS Unlock pattern not found!");
             return;
        }

        uintptr_t injectAddr = pattern.address;
        
        // continue_render: jmp INJECT+15 (0x15)
        s_ContinueRender = injectAddr + 0x15;
        
        // sleep_call: jmp INJECT+C (0x0C)
        s_SleepCall = injectAddr + 0x0C;

        s_FPSUnlockPatch = std::make_unique<FPSUnlockWrapper>(injectAddr);
        
        if (enable)
            s_FPSUnlockPatch->Activate();

        LOG_INFO("[AC2 EaglePatch] FPS Unlock initialized.");
    }

    void SetFPSUnlock(bool enable)
    {
        if (s_FPSUnlockPatch)
            s_FPSUnlockPatch->Toggle(enable);
    }
}