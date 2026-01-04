#pragma once
#include "../Core/BasicTypes.h"
#include "Entity.h"
#include "World.h"
#include "CharacterAI.h"
#include "Components/BipedComponent.h"

namespace AC2
{
    class BhvAssassin
    {
    public:
        char pad_0000[8];           // 0x0000
        Entity* m_pEntity;          // 0x0008
        char pad_000C[4];           // 0x000C
        CharacterAI* m_pCharacterAI;// 0x0010
        char pad_0014[8];           // 0x0014
        bool m_bIsInvisible;        // 0x001C
        char pad_001D[0x27];        // 0x001D
        BipedComponent* m_pBiped;   // 0x0044
        char pad_0048[0x24];        // 0x0048
        World* m_pWorld;            // 0x006C
    };
    assert_offsetof(BhvAssassin, m_pEntity, 0x0008);
    assert_offsetof(BhvAssassin, m_pCharacterAI, 0x0010);
    assert_offsetof(BhvAssassin, m_bIsInvisible, 0x001C);
    assert_offsetof(BhvAssassin, m_pBiped, 0x0044);
    assert_offsetof(BhvAssassin, m_pWorld, 0x006C);
}
