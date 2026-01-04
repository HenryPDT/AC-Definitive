#include "EaglePatch.h"
#include <AutoAssemblerKinda.h>
#include <PatternScanner.h>
#include "log.h"

namespace ACREaglePatch
{
    struct SkipIntroHook : AutoAssemblerCodeHolder_Base
    {
        SkipIntroHook(uintptr_t address) {
            DEFINE_ADDR(hook_addr, address);
            // Patch 0xEB (JMP short)
            hook_addr = { db(0xEB) };
        }
    };

    void InitSkipIntro(uintptr_t baseAddr, GameVersion version)
    {
        auto skipAddr = Utils::PatternScanner::ScanMain("75 0C E8 ?? ?? ?? 00");

        if (skipAddr)
        {
            static AutoAssembleWrapper<SkipIntroHook> hook(skipAddr.As<uintptr_t>());
            hook.Activate();
            LOG_INFO("[EaglePatch] Skip Intro patch applied.");
        }
        else
            LOG_INFO("[EaglePatch] SkipIntro: Pattern NOT found!");
    }
}
