#pragma once
#include "../Core/BasicTypes.h"
#include "Enums/ItemIDs.h"

namespace AC2
{
    /**
     * @brief Standard Inventory Item structure used for Money, Medicine, etc.
     */
    class InventoryItem
    {
    public:
        char pad_0000[0xC];
        ItemID m_ItemID;    // 0x000C
        int32_t m_Count;    // 0x0010 (Matches CE "4 Bytes" at offset 0x10)
        char pad_0014[0x8];
        int32_t m_MaxCount; // 0x001C
    };
    assert_offsetof(InventoryItem, m_ItemID, 0x000C);
    assert_offsetof(InventoryItem, m_Count, 0x0010);
    assert_offsetof(InventoryItem, m_MaxCount, 0x001C);

    /**
     * @brief Specialized Inventory Item for rechargeable containers like Knives.
     * Inheriting caused offset issues, so we define it manually to match CE byte-for-byte.
     */
    class InventoryRechargeable
    {
    public:
        char pad_0000[0xC];
        ItemID m_ItemID;    // 0x000C
        int32_t m_Count;    // 0x0010
        char pad_0014[0x8];
        union {
            int32_t m_MaxCount; // 0x001C
            struct {
                uint8_t m_MaxByte;   // 0x001C (CE: Offset 1C)
                uint8_t m_PadByte;   // 0x001D
                uint8_t m_CountByte; // 0x001E (CE: Offset 1E)
            };
        };
    };
    assert_offsetof(InventoryRechargeable, m_ItemID, 0x000C);
    assert_offsetof(InventoryRechargeable, m_Count, 0x0010);
    assert_offsetof(InventoryRechargeable, m_MaxCount, 0x001C);
    assert_offsetof(InventoryRechargeable, m_MaxByte, 0x001C);
    assert_offsetof(InventoryRechargeable, m_CountByte, 0x001E);

    class InventoryList
    {
    public:
        InventoryItem* m_pMoney;        // 0x0000 (CE: pInventory + 10, 0, 10)
        InventoryItem* m_pSmokeBombs;   // 0x0004 (CE: pInventory + 10, 04, 10)
        char pad_0008[4];
        InventoryItem* m_pMedicine;     // 0x000C (CE: pInventory + 10, 0C, 10)
        InventoryItem* m_pPoison;       // 0x0010
        InventoryItem* m_pBullets;      // 0x0014
        char pad_0018[8];
        InventoryRechargeable* m_pKnives; // 0x0020 (CE: pInventory + 10, 20, 1E/1C)
    };
    assert_offsetof(InventoryList, m_pMoney, 0x0000);
    assert_offsetof(InventoryList, m_pSmokeBombs, 0x0004);
    assert_offsetof(InventoryList, m_pMedicine, 0x000C);
    assert_offsetof(InventoryList, m_pKnives, 0x0020);

    class Inventory
    {
    public:
        char pad_0000[0x10];
        InventoryList* m_pList; // 0x0010 (Matches CE pInventory base + 0x10)
    };
    assert_offsetof(Inventory, m_pList, 0x0010);
}