#pragma once
#include <cstdint>
#include "../EaglePatch.h"

namespace AC2EaglePatch
{
    void InitFPSUnlock(uintptr_t baseAddr, GameVersion version, bool enable);
    void SetFPSUnlock(bool enable);
}
