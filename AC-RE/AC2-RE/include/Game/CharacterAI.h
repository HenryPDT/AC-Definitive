#pragma once
#include "../Core/BasicTypes.h"
#include "PlayerData.h"

namespace AC2
{
    class CharacterAI
    {
    public:
        char pad_0000[0xC];
        float m_Notoriety;              // 0x000C
        char pad_0010[0x48];
        PlayerDataItem* m_pPlayerData;  // 0x0058 (Inventory/SharedData)
    };
    assert_offsetof(CharacterAI, m_Notoriety, 0x000C);
    assert_offsetof(CharacterAI, m_pPlayerData, 0x0058);
}