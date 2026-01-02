#pragma once
#include <cstdint>

namespace ACR
{
    namespace Gear
    {
        class MemHook
        {
        public:
            static MemHook*** pRef;
            static MemHook* GetRef() { return **pRef; }
            virtual void _0() = 0;
            virtual void* Alloc(int, uint32_t, void*, const void*, const char*, const char*, uint32_t, const char*) = 0;
            virtual void _8() = 0;
            virtual void _12() = 0;
            virtual void _16() = 0;
            virtual void Free(int, void*, uint32_t, void*, const char*) = 0;
        };
    }
}
