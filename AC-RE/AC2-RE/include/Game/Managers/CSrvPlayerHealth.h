#pragma once
#include "../../Core/BasicTypes.h"

namespace AC2
{
    /**
     * @brief CSrvPlayerHealth structure containing integer-based health values.
     * Found via the complex Entity chain in the CE collectWhiteRoom script.
     */
    class CSrvPlayerHealth
    {
    public:
        char pad_0000[0x58];
        int32_t m_CurrentHealth;    // 0x0058 (CE: 123_Health.txt - "Type: 4 Bytes")
        int32_t m_MaxHealth;        // 0x005C (CE: 124_Maximum Health.txt - "Type: 4 Bytes")
        int32_t m_ArmorDamage;      // 0x0060 (CE: 125_Armor Damage.txt - "Type: 4 Bytes")
        char pad_0064[0x58];        // 0x0064
        bool m_Desync;              // 0x00BC (CE: 2451_Desync - "Type: Byte")
    };
    assert_offsetof(CSrvPlayerHealth, m_CurrentHealth, 0x0058);
    assert_offsetof(CSrvPlayerHealth, m_MaxHealth, 0x005C);
    assert_offsetof(CSrvPlayerHealth, m_ArmorDamage, 0x0060);
    assert_offsetof(CSrvPlayerHealth, m_Desync, 0x00BC);
}