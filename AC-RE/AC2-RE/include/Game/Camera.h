#pragma once
#include "../Core/BasicTypes.h"

namespace AC2
{
    class Camera
    {
    public:
        char pad_0000[0x30];
        float m_Distance; // 0x0030 (Matches CE 2090_Cam Distance.asm)
    };
    assert_offsetof(Camera, m_Distance, 0x0030);
}
