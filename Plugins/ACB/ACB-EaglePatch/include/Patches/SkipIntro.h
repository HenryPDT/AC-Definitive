#pragma once
#include <cstdint>
#include "../EaglePatch.h"

namespace ACBEaglePatch
{
    void InitSkipIntro(uintptr_t baseAddr, GameVersion version, bool enable);
}
