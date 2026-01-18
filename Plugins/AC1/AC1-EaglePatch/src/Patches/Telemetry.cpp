#include "EaglePatch.h"
#include <AutoAssemblerKinda.h>
#include "log.h"

namespace AC1EaglePatch
{
    // ==========================================================================
    // Disable Telemetry
    // ==========================================================================
    // Patches the telemetry server URL string to disable analytics connections.
    // Pattern: 67 63 6F 6E 6E 65 63 74 2E 75 62 69 2E 63 6F 6D
    //          ("gconnect.ubi.com" in ASCII)
    // Patch:   00 (null byte to terminate string early)
    //
    // Note: Uses allSections=true since this is in the data section, not code
    // ==========================================================================

    DEFINE_DATA_PATCH(Telemetry, "67 63 6F 6E 6E 65 63 74 2E 75 62 69 2E 63 6F 6D", 0, (uint8_t)0x00, true);

    void InitTelemetry(uintptr_t baseAddr, GameVersion version, bool enable)
    {
        if (!Telemetry_Descriptor.IsResolved()) {
            return;
        }

        if (enable)
        {
            HookManager::Install(&Telemetry_Descriptor);
            LOG_INFO("[EaglePatch] Telemetry patch applied.");
        }
    }
}
