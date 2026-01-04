#pragma once
#include "../../Core/BasicTypes.h"
#include "../Entity.h"
#include "../../Scimitar/math.h"

namespace AC2
{
    class BipedComponent
    {
    public:
        char pad_0000[0x8];
        Entity* m_pOwnerEntity;         // 0x0008
        char pad_000C[4];               // 0x000C
        char pad_0010[0xA];
        uint8_t m_ActionFlags;          // 0x001A (0 = climbing/swimming/etc?)
        char pad_001B[0x85];
        Scimitar::Vector4 m_Speed;      // 0x00A0 (X, Y components typically hold speed)
    };
    assert_offsetof(BipedComponent, m_pOwnerEntity, 0x0008);
    assert_offsetof(BipedComponent, m_ActionFlags, 0x001A);
    assert_offsetof(BipedComponent, m_Speed, 0x00A0);
}