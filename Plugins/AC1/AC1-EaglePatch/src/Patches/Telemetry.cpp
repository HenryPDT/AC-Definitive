#include "EaglePatch.h"
#include <AutoAssemblerKinda.h>
#include <PatternScanner.h>

namespace AC1EaglePatch
{
    struct TelemetryHook : AutoAssemblerCodeHolder_Base
    {
        TelemetryHook(uintptr_t address) {
            DEFINE_ADDR(hook_addr, address);
            // Patch 0 to disable
            hook_addr = { db(0x00) };
        }
    };

    void InitTelemetry(uintptr_t baseAddr, GameVersion version)
    {
        auto result = Utils::PatternScanner::ScanMain("67 63 6F 6E 6E 65 63 74 2E 75 62 69 2E 63 6F 6D", true);

        if (result)
        {
            static AutoAssembleWrapper<TelemetryHook> hook(result.As<uintptr_t>());
            hook.Activate();
            if (g_loader_ref) g_loader_ref->LogToConsole("[EaglePatch] Telemetry patch applied.");
        }
        else if (g_loader_ref) g_loader_ref->LogToConsole("[EaglePatch] Telemetry: Pattern NOT found!");
    }
}
