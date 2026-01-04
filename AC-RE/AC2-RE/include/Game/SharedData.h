#pragma once
#include "../Core/BasicTypes.h"

namespace AC2
{
    class SharedData
    {
    public:
        char pad_0000[0x20];
        uint8_t m_GodModeFlags; // 0x0020 (0x80 = Normal, 0x81 = GodMode)
    };
    assert_offsetof(SharedData, m_GodModeFlags, 0x0020);
}
