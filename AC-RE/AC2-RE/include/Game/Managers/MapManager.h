#pragma once
#include "../../Core/BasicTypes.h"
#include "../../Scimitar/math.h"

namespace AC2
{
    class MapWaypoint
    {
    public:
        char pad_0000[0x10];
        Scimitar::Vector3 Position; // 0x0010 (X, Y, Z/IconZ)
    };
    assert_offsetof(MapWaypoint, Position, 0x0010);

    class MapManager
    {
    public:
        char pad_0000[0x18];
        MapWaypoint* m_pCurrentWaypoint; // 0x0018
        char pad_001C[0x8];
        uint8_t m_Flags;                 // 0x0024
    };
    assert_offsetof(MapManager, m_pCurrentWaypoint, 0x0018);
    assert_offsetof(MapManager, m_Flags, 0x0024);
}
