#pragma once
#include <cstdint>
#include "../EaglePatch.h"

namespace ACREaglePatch
{
    void InitSkipIntro(uintptr_t baseAddr, GameVersion version, bool enable);
}
