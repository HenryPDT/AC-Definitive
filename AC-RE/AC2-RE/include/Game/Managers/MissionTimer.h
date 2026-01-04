#pragma once
#include "../../Core/BasicTypes.h"

namespace AC2
{
    // Reversed from CE Script 2218_Mission Timer.asm
    class MissionTimer
    {
    public:
        char pad_0000[0x34];
        float m_MaxTimeLimit;   // 0x0034 (2227_Max Time limit)
        char pad_0038[0x8];
        uint32_t m_SyncTime;    // 0x0040 (Target for freezing)
        char pad_0044[0x4];
        int32_t m_RemainingTime;// 0x0048 (14475_Remaining time)
        char pad_004C[0x4];
        bool m_IsPaused;        // 0x0050 (2219_Pause system flag)
        char pad_0051[0x3];
        uint32_t* m_pStartTime; // 0x0054 (2221_Start-time is [m_pStartTime])
    };
    assert_offsetof(MissionTimer, m_MaxTimeLimit, 0x0034);
    assert_offsetof(MissionTimer, m_SyncTime, 0x0040);
    assert_offsetof(MissionTimer, m_RemainingTime, 0x0048);
    assert_offsetof(MissionTimer, m_IsPaused, 0x0050);
    assert_offsetof(MissionTimer, m_pStartTime, 0x0054);

    extern MissionTimer* g_pMissionTimer;
    MissionTimer* GetMissionTimer();
}