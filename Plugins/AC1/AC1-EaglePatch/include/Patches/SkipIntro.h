#pragma once
#include <cstdint>
#include "../EaglePatch.h"

namespace AC1EaglePatch
{
    void InitSkipIntro(uintptr_t baseAddr, GameVersion version, bool enable);
}
