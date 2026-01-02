#include "PatternScanner.h"
#include "log.h"
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>

namespace Utils
{
    // --- Helper for Safe Memory Access ---
    static bool IsSafeRead(const void* ptr, size_t size)
    {
        if (!ptr) return false;
        MEMORY_BASIC_INFORMATION mbi = { 0 };
        if (VirtualQuery(ptr, &mbi, sizeof(mbi)) == 0) return false;
        
        // Check if memory is committed and readable
        if (!(mbi.State & MEM_COMMIT)) return false;
        if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return false;

        // Check bounds within the region
        if ((uintptr_t)ptr + size > (uintptr_t)mbi.BaseAddress + mbi.RegionSize) return false;

        return true;
    }

    // --- ScanResult Implementation ---

    ScanResult ScanResult::Offset(intptr_t offset) const {
        if (!found) return *this;
        return { address + offset, true };
    }

    ScanResult ScanResult::Dereference(intptr_t offset) const {
        if (!found) return *this;
        uintptr_t targetAddr = address + offset;
        if (!IsSafeRead((void*)targetAddr, sizeof(uintptr_t))) return { 0, false };
        return { *(uintptr_t*)targetAddr, true };
    }

    ScanResult ScanResult::ResolveRelative(int instructionSize, int offsetToDisplacement) const {
        if (!found) return *this;
        
        uintptr_t instructionEnd = address + instructionSize;
        int32_t displacement = 0;
        
        // Address of the displacement value
        void* dispAddr = (void*)(address + offsetToDisplacement);

        if (!IsSafeRead(dispAddr, sizeof(int32_t))) return { 0, false };
        
        displacement = *(int32_t*)dispAddr;
        return { instructionEnd + displacement, true };
    }

    ScanResult ScanResult::ResolvePointerChain(const std::vector<int32_t>& offsets) const {
        if (!found) return *this;
        
        uintptr_t current = address;
        for (size_t i = 0; i < offsets.size(); ++i) {
            if (!IsSafeRead((void*)current, sizeof(uintptr_t))) return { 0, false };
            
            // Dereference
            current = *(uintptr_t*)current;
            if (current == 0) return { 0, false };

            // Add offset
            current += offsets[i];
        }
        return { current, true };
    }

    ScanResult ScanResult::ScanRelative(std::string_view signature, intptr_t range) const {
        if (!found || range == 0) return *this;

        auto pattern = PatternScanner::ParseSignature(signature);
        if (pattern.empty()) return { 0, false };

        size_t patternSize = pattern.size();
        int firstByte = pattern[0];
        size_t absRange = std::abs(range);
        int step = (range > 0) ? 1 : -1;

        for (size_t i = 0; i < absRange; ++i) {
            uintptr_t currentAddr = address + (i * step);
            const uint8_t* currentPos = (const uint8_t*)currentAddr;

            if (firstByte != -1 && *currentPos != firstByte) continue;

            bool matched = true;
            for (size_t j = 1; j < patternSize; ++j) {
                if (pattern[j] != -1 && pattern[j] != currentPos[j]) {
                    matched = false;
                    break;
                }
            }

            if (matched) return { currentAddr, true };
        }
        return { 0, false };
    }

    ScanResult ScanResult::AlignToFunctionStart() const {
        if (!found) return *this;
        
        // Get the bounds of the current memory page
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery((void*)address, &mbi, sizeof(mbi)) == 0) return { 0, false };

        const uint8_t* curr = (const uint8_t*)address;
        const uint8_t* pageBase = (const uint8_t*)mbi.BaseAddress;

        // Scan backwards
        for(int i = 0; i < 2048; i++) {
            const uint8_t* target = curr - i;

            // Stop if we hit the beginning of the allocated page
            if (target < pageBase) break;

            // Check for "CC CC" (Padding)
            // Ensure we don't read before pageBase
            if (target > pageBase && target[-1] == 0xCC && target[0] != 0xCC) {
                return { (uintptr_t)(target), true };
            }
            
            // Check for "55 8B EC" (Function Prologue)
            if (i + 2 < 2048 && target[0] == 0x55 && target[1] == 0x8B && target[2] == 0xEC) {
                 return { (uintptr_t)(target), true };
            }
        }
        return { 0, false };
    }

    // --- PatternScanner Implementation ---

    std::vector<int> PatternScanner::ParseSignature(std::string_view signature)
    {
        std::vector<int> pattern;
        std::stringstream ss;
        ss.str(std::string(signature));
        std::string byteStr;

        while (ss >> byteStr)
        {
            if (byteStr == "?" || byteStr == "??")
            {
                pattern.push_back(-1); // Wildcard
            }
            else
            {
                try {
                    pattern.push_back(std::stoi(byteStr, nullptr, 16));
                }
                catch (...) {
                    LOG_ERROR("[PatternScanner] Invalid byte in signature: %s", byteStr.c_str());
                    pattern.push_back(-1);
                }
            }
        }
        return pattern;
    }

    ScanResult PatternScanner::ScanInternal(const uint8_t* start, size_t size, const std::vector<int>& pattern)
    {
        if (!start || size == 0 || pattern.empty()) return { 0, false };

        int firstByte = pattern[0];
        size_t patternSize = pattern.size();
        size_t scanEnd = size - patternSize;

        for (size_t i = 0; i <= scanEnd; ++i)
        {
            // Optimization: If first byte is not a wildcard, use memchr to skip
            if (firstByte != -1) {
                const void* found = std::memchr(start + i, firstByte, size - i);
                if (!found) return { 0, false };
                
                // Update index to the found position
                i = (size_t)((const uint8_t*)found - start);
                
                // Re-check bound after jump
                if (i > scanEnd) return { 0, false };
            }

            bool found = true;
            for (size_t j = 1; j < patternSize; ++j)
            {
                if (pattern[j] != -1 && pattern[j] != start[i + j])
                {
                    found = false;
                    break;
                }
            }
            
            if (found) {
                return { reinterpret_cast<uintptr_t>(start + i), true };
            }
        }
        return { 0, false };
    }

    ScanResult PatternScanner::Scan(HMODULE module, std::string_view signature, bool allSections)
    {
        if (!module) return { 0, false };

        auto dosHeader = (PIMAGE_DOS_HEADER)module;
        if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) return { 0, false };

        auto ntHeaders = (PIMAGE_NT_HEADERS)((uint8_t*)module + dosHeader->e_lfanew);
        if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) return { 0, false };

        auto pattern = ParseSignature(signature);
        if (pattern.empty()) return { 0, false };

        uint8_t* imageBase = (uint8_t*)module;
        auto sectionHeader = IMAGE_FIRST_SECTION(ntHeaders);
        
        for (int i = 0; i < ntHeaders->FileHeader.NumberOfSections; ++i, ++sectionHeader)
        {
            bool shouldScan = (sectionHeader->Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0;
            if (allSections) shouldScan |= (sectionHeader->Characteristics & IMAGE_SCN_MEM_READ) != 0;

            if (shouldScan)
            {
                auto result = ScanInternal(
                    imageBase + sectionHeader->VirtualAddress,
                    sectionHeader->Misc.VirtualSize,
                    pattern
                );
                if (result) return result;
            }
        }

        return { 0, false };
    }

    ScanResult PatternScanner::ScanMain(std::string_view signature, bool allSections)
    {
        return Scan(GetModuleHandle(NULL), signature, allSections);
    }

    ScanResult PatternScanner::ScanModule(std::string_view moduleName, std::string_view signature, bool allSections)
    {
        return Scan(GetModuleHandleA(moduleName.data()), signature, allSections);
    }

    ScanResult PatternScanner::ScanSection(const char* sectionName, std::string_view signature)
    {
        HMODULE module = GetModuleHandle(NULL);
        if (!module) return { 0, false };

        auto dosHeader = (PIMAGE_DOS_HEADER)module;
        auto ntHeaders = (PIMAGE_NT_HEADERS)((uint8_t*)module + dosHeader->e_lfanew);
        auto sectionHeader = IMAGE_FIRST_SECTION(ntHeaders);

        auto pattern = ParseSignature(signature);
        if (pattern.empty()) return { 0, false };

        for (int i = 0; i < ntHeaders->FileHeader.NumberOfSections; ++i, ++sectionHeader)
        {
            // Compare section name (max 8 chars)
            if (strncmp((const char*)sectionHeader->Name, sectionName, 8) == 0)
            {
                // Only scan this section
                return ScanInternal(
                    (uint8_t*)module + sectionHeader->VirtualAddress,
                    sectionHeader->Misc.VirtualSize,
                    pattern
                );
            }
        }
        return { 0, false };
    }

    ScanResult PatternScanner::ScanCandidates(const std::vector<std::string_view>& signatures, bool allSections)
    {
        for (const auto& sig : signatures) {
            auto result = ScanMain(sig, allSections);
            if (result) return result;
        }
        return { 0, false };
    }

    std::vector<ScanResult> PatternScanner::ScanAll(HMODULE module, std::string_view signature, bool allSections)
    {
        std::vector<ScanResult> results;
        if (!module) return results;

        auto dosHeader = (PIMAGE_DOS_HEADER)module;
        if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) return results;

        auto ntHeaders = (PIMAGE_NT_HEADERS)((uint8_t*)module + dosHeader->e_lfanew);
        if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) return results;

        auto pattern = ParseSignature(signature);
        if (pattern.empty()) return results;

        uint8_t* imageBase = (uint8_t*)module;
        auto sectionHeader = IMAGE_FIRST_SECTION(ntHeaders);
        
        for (int i = 0; i < ntHeaders->FileHeader.NumberOfSections; ++i, ++sectionHeader)
        {
            bool shouldScan = (sectionHeader->Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0;
            if (allSections) shouldScan |= (sectionHeader->Characteristics & IMAGE_SCN_MEM_READ) != 0;

            if (shouldScan)
            {
                 uint8_t* start = imageBase + sectionHeader->VirtualAddress;
                 size_t size = sectionHeader->Misc.VirtualSize;
                 
                 if (!start || size == 0) continue;

                 int firstByte = pattern[0];
                 size_t patternSize = pattern.size();
                 size_t scanEnd = size - patternSize;

                 for (size_t k = 0; k <= scanEnd; ++k)
                 {
                    if (firstByte != -1 && start[k] != firstByte) continue;

                    bool found = true;
                    for (size_t j = 1; j < patternSize; ++j)
                    {
                        if (pattern[j] != -1 && pattern[j] != start[k + j])
                        {
                            found = false;
                            break;
                        }
                    }
                    if (found)
                    {
                        results.push_back({ reinterpret_cast<uintptr_t>(start + k), true });
                        k += patternSize - 1; 
                    }
                 }
            }
        }
        return results;
    }

    ScanResult PatternScanner::ScanRange(const uint8_t* start, size_t size, std::string_view signature)
    {
        return ScanInternal(start, size, ParseSignature(signature));
    }
}