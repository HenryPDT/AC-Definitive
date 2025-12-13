#include "../EaglePatch.h"
#include <AutoAssemblerKinda.h>

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
        uintptr_t telemetryAddr = 0;
        switch (version)
        {
        case GameVersion::Version1: // DX10
            telemetryAddr = baseAddr + 0x130D798;
            break;
        case GameVersion::Version2: // DX9
            telemetryAddr = baseAddr + 0x13382D8;
            break;
        default: return;
        }

        static AutoAssembleWrapper<TelemetryHook> hook(telemetryAddr);
        hook.Activate();
        
        if (g_loader_ref) g_loader_ref->LogToConsole("[EaglePatch] Telemetry disabled.");
    }
}
