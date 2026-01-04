#pragma once
#include "../Core/BasicTypes.h"
#include "../Scimitar/math.h"

namespace AC2
{
    class EntityVisual
    {
    public:
        char pad_0000[0xB4];
        float m_Scale; // 0x00B4
    };

    class Entity
    {
    public:
        char pad_0000[0x20];             // 0x0000
        Scimitar::Vector4 Rotation;      // 0x0020
        char pad_0030[0x10];             // 0x0030
        Scimitar::Vector3 Position;      // 0x0040
        char pad_004C[0x28];             // 0x4C
        void* m_pHealthChainRoot;        // 0x0074 (CE: [[[[[Entity+74]+64]+10]+30]+40])
        char pad_0078[4];                // 0x0078
        float Scale;                     // 0x007C
        char pad_0080[0x38];             // 0x0080
        EntityVisual* m_pVisual;         // 0x00B8
        char pad_00BC[0x1C];             // 0x00BC
        uint8_t m_PlayerID;              // 0x00D8
    };
    assert_offsetof(Entity, m_pHealthChainRoot, 0x0074);
    assert_offsetof(Entity, Scale, 0x007C);
}