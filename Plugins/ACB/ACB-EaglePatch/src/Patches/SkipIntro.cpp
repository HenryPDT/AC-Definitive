#include "../EaglePatch.h"
#include <AutoAssemblerKinda.h>

namespace ACBEaglePatch
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
        uintptr_t skipAddr = 0;
        switch (version)
        {
        case GameVersion::Version1:
            skipAddr = baseAddr + 0xCF93;
            break;
        case GameVersion::Version2:
            skipAddr = baseAddr + 0xC0AC;
            break;
        default: return;
        }

        static AutoAssembleWrapper<SkipIntroHook> hook(skipAddr);
        hook.Activate();
        
        if (g_loader_ref) g_loader_ref->LogToConsole("[EaglePatch] Skip Intro patch applied.");
    }
}
