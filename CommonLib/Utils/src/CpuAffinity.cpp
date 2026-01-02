#include "CpuAffinity.h"
#include <windows.h>
#include "log.h"
#include <vector>

namespace CpuAffinity
{
    uint64_t GetSystemAffinityMask()
    {
        HANDLE hProcess = GetCurrentProcess();
        DWORD_PTR processMask = 0, systemMask = 0;
        if (!GetProcessAffinityMask(hProcess, &processMask, &systemMask))
        {
            return 0;
        }
        return (uint64_t)systemMask;
    }

    uint64_t GetCurrentProcessMask()
    {
        HANDLE hProcess = GetCurrentProcess();
        DWORD_PTR processMask = 0, systemMask = 0;
        if (!GetProcessAffinityMask(hProcess, &processMask, &systemMask))
        {
            return 0;
        }
        return (uint64_t)processMask;
    }

    void Apply(uint64_t mask)
    {
        if (mask == 0) 
        {
            LOG_WARN("CpuAffinity: Attempted to set empty mask. Ignoring.");
            return;
        }

        HANDLE hProcess = GetCurrentProcess();


        if (!SetProcessAffinityMask(hProcess, (DWORD_PTR)mask))
        {
            LOG_ERROR("CpuAffinity: Failed to set affinity mask. Error: %lu", GetLastError());
        }
        else
        {
            // Count set bits for logging
            int count = 0;
            for (uint64_t i = mask; i; i &= (i - 1)) count++;
            LOG_INFO("CpuAffinity: Applied mask 0x%llX (Active Cores: %d).", mask, count);
        }
    }
}
