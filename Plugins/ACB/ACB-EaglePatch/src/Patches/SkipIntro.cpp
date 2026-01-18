#include "EaglePatch.h"
#include <AutoAssemblerKinda.h>
#include "log.h"

namespace ACBEaglePatch
{
    // ==========================================================================
    // Skip Intro Videos
    // ==========================================================================
    // Patches the conditional jump (JNZ -> JMP) to always skip intro video playback.
    // Pattern: 75 0C E8 ?? ?? ?? 00 8B C8  (JNZ +0x0C followed by a CALL)
    // Patch:   EB                    (JMP short - unconditional)
    // ==========================================================================

    DEFINE_DATA_PATCH(SkipIntro, "75 0C E8 ?? ?? ?? 00 8B C8", 0, (uint8_t)0xEB);

    void InitSkipIntro(uintptr_t baseAddr, GameVersion version, bool enable)
    {
        if (!SkipIntro_Descriptor.IsResolved()) {
            return;
        }

        if (enable)
        {
            HookManager::Install(&SkipIntro_Descriptor);
            LOG_INFO("[EaglePatch] Skip Intro patch applied.");
        }
    }
}
