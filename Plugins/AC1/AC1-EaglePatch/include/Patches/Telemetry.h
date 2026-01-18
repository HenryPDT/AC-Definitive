#pragma once
#include <cstdint>
#include "../EaglePatch.h"

namespace AC1EaglePatch
{
    void InitTelemetry(uintptr_t baseAddr, GameVersion version, bool enable);
}
