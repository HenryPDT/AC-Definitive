#pragma once
#include <cstdint>
#include "../EaglePatch.h"

namespace AC2EaglePatch
{
    void InitGraphics(uintptr_t baseAddr, GameVersion version, bool shadows, bool drawDistance);
    void SetShadowMapResolution(bool enable);
    void SetDrawDistance(bool enable);
}
