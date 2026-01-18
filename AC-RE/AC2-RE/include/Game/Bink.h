#pragma once
#include "../Core/BasicTypes.h"

namespace AC2
{
    // Reversed from CE Files: 95197_BinkInfo.txt
    class Bink
    {
    public:
        uint32_t m_Width;           // 0x0000
        uint32_t m_Height;          // 0x0004
        uint32_t m_FrameCount;      // 0x0008
        uint32_t m_CurrentFrame;    // 0x000C
        uint32_t m_LastFrame;       // 0x0010
        uint32_t m_FpsMultiplier;   // 0x0014
        uint32_t m_FpsDivisor;      // 0x0018
        char pad_001C[4];           // 0x001C
        uint32_t m_Flags;           // 0x0020
    };
    assert_offsetof(Bink, m_Width, 0x0000);
    assert_offsetof(Bink, m_FrameCount, 0x0008);
    assert_offsetof(Bink, m_CurrentFrame, 0x000C);
    assert_offsetof(Bink, m_Flags, 0x0020);
}