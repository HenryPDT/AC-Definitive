#include "../EaglePatch.h"
#include <AutoAssemblerKinda.h>

namespace AC2EaglePatch
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
        case GameVersion::Version1: // Uplay
            skipAddr = baseAddr + 0x1494E;
            break;
        case GameVersion::Version2: // Retail 1.01
            skipAddr = baseAddr + 0x148EE;
            break;
        default: return;
        }

        static AutoAssembleWrapper<SkipIntroHook> hook(skipAddr);
        hook.Activate();
        
        if (g_loader_ref) g_loader_ref->LogToConsole("[EaglePatch] Skip Intro patch applied.");
    }
}
