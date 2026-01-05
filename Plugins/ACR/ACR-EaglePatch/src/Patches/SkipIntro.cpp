#include "EaglePatch.h"
#include <AutoAssemblerKinda.h>
#include "log.h"

namespace ACREaglePatch
{
    // ==========================================================================
    // Skip Intro Videos
    // ==========================================================================
    // Patches the conditional jump (JNZ -> JMP) to always skip intro video playback.
    // Pattern: 75 0C E8 ?? ?? ?? 00  (JNZ +0x0C followed by a CALL)
    // Patch:   EB                    (JMP short - unconditional)
    // ==========================================================================

    DEFINE_DATA_PATCH(SkipIntro, "75 0C E8 ?? ?? ?? 00", 0, (uint8_t)0xEB);

    void InitSkipIntro(uintptr_t baseAddr, GameVersion version)
    {
        if (!HookManager::Resolve(&SkipIntro_Descriptor)) {
            LOG_INFO("[EaglePatch] SkipIntro: Pattern NOT found!");
            return;
        }

        HookManager::Install(&SkipIntro_Descriptor);
        LOG_INFO("[EaglePatch] Skip Intro patch applied.");
    }
}
