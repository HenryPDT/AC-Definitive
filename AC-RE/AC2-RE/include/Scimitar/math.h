#pragma once

#include "../Core/BasicTypes.h"

namespace AC2::Scimitar
{
    struct Vector3
    {
        float x, y, z;
    };
    assert_sizeof(Vector3, 0xC);

    struct Vector4
    {
        float x, y, z, w;
    };
    assert_sizeof(Vector4, 0x10);
}
