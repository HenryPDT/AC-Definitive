#pragma once
#include "../../Core/BasicTypes.h"
#include "../Enums/ItemIDs.h"

namespace AC2
{
    class PlayerProfile
    {
    public:
        char pad_0000[0xC];
        ItemID m_PlayerID; // 0x000C (Matches ItemID enum)
    };
    assert_offsetof(PlayerProfile, m_PlayerID, 0x000C);

    class PlayerAttribute
    {
    public:
        char pad_0000[0x8];
        PlayerProfile* m_pPlayerProfile; // 0x0008
    };
    assert_offsetof(PlayerAttribute, m_pPlayerProfile, 0x0008);

    class ProgressionManager
    {
    public:
        char pad_0000[0x70];
        PlayerProfile* m_pSelectedProfile;     // 0x0070 (Pointer to the active profile struct)
        PlayerAttribute** m_pPlayerAttributes; // 0x0074 (Array of pointers)
        char pad_0078[2];
        int16_t m_AttributeCount;              // 0x007A
    };
    assert_offsetof(ProgressionManager, m_pSelectedProfile, 0x0070);
    assert_offsetof(ProgressionManager, m_pPlayerAttributes, 0x0074);
    assert_offsetof(ProgressionManager, m_AttributeCount, 0x007A);
}
