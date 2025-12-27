#pragma once
#include <cstdint>

namespace CpuAffinity
{
    uint64_t GetSystemAffinityMask();
    void Apply(uint64_t mask);
}