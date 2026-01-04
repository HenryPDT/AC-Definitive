#pragma once
#include "../../Core/BasicTypes.h"

namespace AC2
{
    class TimeOfDayManager
    {
    public:
        char pad_0000[0x58];
        bool m_IsPaused;       // 0x0058 (14612_Pause time)
        char pad_0059[0x47];
        float m_TimeScale;     // 0x00A0 (Divisor/Delay - 14496_Time Of Day.asm uses [esi+A0])
    };
    assert_offsetof(TimeOfDayManager, m_IsPaused, 0x0058);
    assert_offsetof(TimeOfDayManager, m_TimeScale, 0x00A0);

    // CurrentTime is global, not in this struct (accessed via g_Offsets.pTimeOfDay_CurrentTime_Root)
}
