#pragma once
#include <cstdint>
#include "../EaglePatch.h"

namespace AC1EaglePatch
{
    void InitGraphics(uintptr_t baseAddr, GameVersion version, bool enableMSAAFix, bool fixDX10Resolution);
}
