#pragma once
#include "../Core/BasicTypes.h"
#include "Inventory.h"
#include "SharedData.h"

namespace AC2
{
    /**
     * @brief Container for inventory and shared flags.
     * Reached via CharacterAI + 0x58.
     */
    class PlayerDataItem
    {
    public:
        char pad_0000[0x8];
        SharedData* m_pSharedData;  // 0x0008
        Inventory* m_pInventory;    // 0x000C
    };
    assert_offsetof(PlayerDataItem, m_pSharedData, 0x0008);
    assert_offsetof(PlayerDataItem, m_pInventory, 0x000C);
}