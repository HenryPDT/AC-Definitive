#pragma once
#include "../Core/BasicTypes.h"

namespace AC2
{
    // Reversed from CE Script 16909_Manage Speed Multiplier System.asm
    class SpeedSystem
    {
    public:
        char pad_0000[0x28];
        float m_GlobalMultiplier;   // 0x0028 (16911_Speed Multiplier)
        char pad_002C[0x10];
        bool m_IsEnabled;           // 0x003C (16908_Dis_Enable Multiplier)
    };
    assert_offsetof(SpeedSystem, m_GlobalMultiplier, 0x0028);
    assert_offsetof(SpeedSystem, m_IsEnabled, 0x003C);
}