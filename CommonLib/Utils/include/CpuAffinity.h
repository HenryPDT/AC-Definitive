#pragma once
#include <cstdint>

namespace CpuAffinity
{
    uint64_t GetSystemAffinityMask();
    uint64_t GetCurrentProcessMask();
    void Apply(uint64_t mask);
}