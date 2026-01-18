#include "PatternScanner.h"
#include "log.h"
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>

namespace AutoAssemblerKinda
{
    // --- Helper for Safe Memory Access ---
    bool IsSafeRead(const void* ptr, size_t size)
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

    // --- PatternScanner Implementation ---

    PatternScanner PatternScanner::Offset(intptr_t offset) const {
        if (!m_Found) return *this;
        return { m_Address + offset, true };
    }

    PatternScanner PatternScanner::Dereference(intptr_t offset) const {
        if (!m_Found) return *this;
        uintptr_t targetAddr = m_Address + offset;
        if (!IsSafeRead((void*)targetAddr, sizeof(uintptr_t))) return { 0, false };
        return { *(uintptr_t*)targetAddr, true };
    }

    PatternScanner PatternScanner::ResolveRelative(int instructionSize, int offsetToDisplacement) const {
        if (!m_Found) return *this;
        
        uintptr_t instructionEnd = m_Address + instructionSize;
        int32_t displacement = 0;
        
        // Address of the displacement value
        void* dispAddr = (void*)(m_Address + offsetToDisplacement);

        if (!IsSafeRead(dispAddr, sizeof(int32_t))) return { 0, false };
        
        displacement = *(int32_t*)dispAddr;
        return { instructionEnd + displacement, true };
    }

    PatternScanner PatternScanner::ResolvePointerChain(const std::vector<int32_t>& offsets) const {
        if (!m_Found) return *this;
        
        uintptr_t current = m_Address;
        for (size_t i = 0; i < offsets.size(); ++i) {
            current += offsets[i];
            if (!IsSafeRead((void*)current, sizeof(uintptr_t))) return { 0, false };
            current = *(uintptr_t*)current;
            if (current == 0) return { 0, false };
        }
        return { current, true };
    }

    static bool IsAbsoluteModRM(uint8_t modrm) {
        return (modrm & 0xC7) == 0x05; 
    }

    PatternScanner PatternScanner::ExtractAbsoluteAddress(intptr_t instructionOffset) const {
        if (!m_Found) return *this;

        uintptr_t instrAddr = m_Address + instructionOffset;
        if (!IsSafeRead((void*)instrAddr, 10)) return { 0, false }; 

        const uint8_t* pInstr = reinterpret_cast<const uint8_t*>(instrAddr);
        uintptr_t extracted = 0;

        switch (pInstr[0]) {
            case 0xA1: case 0xA3: // MOV EAX, [disp32] / MOV [disp32], EAX
                extracted = *reinterpret_cast<const uint32_t*>(pInstr + 1);
                break;
            case 0x8B: case 0x89: case 0x8D: // MOV/LEA
                if (IsAbsoluteModRM(pInstr[1])) extracted = *reinterpret_cast<const uint32_t*>(pInstr + 2);
                break;
            case 0x0F: // Multi-byte
                if ((pInstr[1] == 0x10 || pInstr[1] == 0x11 || pInstr[1] == 0x28 || pInstr[1] == 0x29 || pInstr[1] == 0xB6 || pInstr[1] == 0xB7) && IsAbsoluteModRM(pInstr[2])) {
                    extracted = *reinterpret_cast<const uint32_t*>(pInstr + 3);
                }
                break;
            case 0xF3: // SSE Prefix
                if (pInstr[1] == 0x0F && (pInstr[2] == 0x10 || pInstr[2] == 0x11) && IsAbsoluteModRM(pInstr[3])) {
                    extracted = *reinterpret_cast<const uint32_t*>(pInstr + 4);
                }
                break;
            default:
                LOG_ERROR("[PatternScanner] Unsupported opcode 0x%02X for absolute address extraction at 0x%p", pInstr[0], (void*)instrAddr);
                return { 0, false };
        }

        if (extracted != 0) return { extracted, true };
        return { 0, false };
    }

    PatternScanner PatternScanner::ScanRelative(std::string_view signature, intptr_t range) const {
        if (!m_Found || range == 0) return *this;

        auto pattern = ParseSignature(signature);
        if (pattern.empty()) return { 0, false };

        size_t patternSize = pattern.size();
        int firstByte = pattern[0];
        size_t absRange = std::abs(range);
        int step = (range > 0) ? 1 : -1;

        for (size_t i = 0; i < absRange; ++i) {
            uintptr_t currentAddr = m_Address + (i * step);
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

    PatternScanner PatternScanner::AlignToFunctionStart() const {
        if (!m_Found) return *this;
        
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery((void*)m_Address, &mbi, sizeof(mbi)) == 0) return { 0, false };

        const uint8_t* curr = (const uint8_t*)m_Address;
        const uint8_t* pageBase = (const uint8_t*)mbi.BaseAddress;

        for(int i = 0; i < 2048; i++) {
            const uint8_t* target = curr - i;
            if (target < pageBase) break;
            
            if (target > pageBase && target[-1] == 0xCC && target[0] != 0xCC) return { (uintptr_t)(target), true };
            if (i + 2 < 2048 && target[0] == 0x55 && target[1] == 0x8B && target[2] == 0xEC) return { (uintptr_t)(target), true };
        }
        return { 0, false };
    }

    std::vector<int> PatternScanner::ParseSignature(std::string_view signature)
    {
        std::vector<int> pattern;
        std::stringstream ss;
        ss.str(std::string(signature));
        std::string byteStr;

        while (ss >> byteStr)
        {
            if (byteStr == "?" || byteStr == "??") pattern.push_back(-1);
            else {
                try { pattern.push_back(std::stoi(byteStr, nullptr, 16)); }
                catch (...) { LOG_ERROR("[PatternScanner] Invalid byte: %s", byteStr.c_str()); pattern.push_back(-1); }
            }
        }
        return pattern;
    }

    PatternScanner PatternScanner::ScanInternal(const uint8_t* start, size_t size, const std::vector<int>& pattern)
    {
        if (!start || size == 0 || pattern.empty()) return { 0, false };

        int firstByte = pattern[0];
        size_t patternSize = pattern.size();
        size_t scanEnd = size - patternSize;

        for (size_t i = 0; i <= scanEnd; ++i)
        {
            if (firstByte != -1) {
                const void* found = std::memchr(start + i, firstByte, size - i);
                if (!found) return { 0, false };
                i = (size_t)((const uint8_t*)found - start);
                if (i > scanEnd) return { 0, false };
            }

            bool found = true;
            for (size_t j = 1; j < patternSize; ++j) {
                if (pattern[j] != -1 && pattern[j] != start[i + j]) {
                    found = false;
                    break;
                }
            }
            if (found) return { reinterpret_cast<uintptr_t>(start + i), true };
        }
        return { 0, false };
    }

    PatternScanner PatternScanner::Scan(HMODULE module, std::string_view signature, bool allSections, bool requireUnique)
    {
        if (!module) return { 0, false };

        if (requireUnique) {
            auto allMatches = ScanAll(module, signature, allSections);
            if (allMatches.size() == 1) return allMatches[0];
            if (allMatches.size() > 1) {
                LOG_ERROR("Multiple matches (%zu) found for signature: %.*s", allMatches.size(), (int)signature.length(), signature.data());
            } else {
                LOG_ERROR("Pattern not found for signature: %.*s", (int)signature.length(), signature.data());
            }
            return { 0, false };
        }

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
                auto result = ScanInternal(imageBase + sectionHeader->VirtualAddress, sectionHeader->Misc.VirtualSize, pattern);
                if (result) return result;
            }
        }
        return { 0, false };
    }

    PatternScanner PatternScanner::ScanMain(std::string_view signature, bool allSections, bool requireUnique) {
        return Scan(GetModuleHandle(NULL), signature, allSections, requireUnique);
    }

    PatternScanner PatternScanner::ScanModule(std::string_view moduleName, std::string_view signature, bool allSections, bool requireUnique) {
        return Scan(GetModuleHandleA(moduleName.data()), signature, allSections, requireUnique);
    }

    PatternScanner PatternScanner::ScanSection(const char* sectionName, std::string_view signature)
    {
        HMODULE module = GetModuleHandle(NULL);
        if (!module) return { 0, false };

        auto dosHeader = (PIMAGE_DOS_HEADER)module;
        auto ntHeaders = (PIMAGE_NT_HEADERS)((uint8_t*)module + dosHeader->e_lfanew);
        auto sectionHeader = IMAGE_FIRST_SECTION(ntHeaders);

        auto pattern = ParseSignature(signature);
        if (pattern.empty()) return { 0, false };

        for (int i = 0; i < ntHeaders->FileHeader.NumberOfSections; ++i, ++sectionHeader) {
            if (strncmp((const char*)sectionHeader->Name, sectionName, 8) == 0) {
                return ScanInternal((uint8_t*)module + sectionHeader->VirtualAddress, sectionHeader->Misc.VirtualSize, pattern);
            }
        }
        return { 0, false };
    }

    PatternScanner PatternScanner::ScanCandidates(const std::vector<std::string_view>& signatures, bool allSections) {
        for (const auto& sig : signatures) {
            auto result = ScanMain(sig, allSections);
            if (result) return result;
        }
        return { 0, false };
    }

    std::vector<PatternScanner> PatternScanner::ScanAll(HMODULE module, std::string_view signature, bool allSections)
    {
        std::vector<PatternScanner> results;
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
                    for (size_t j = 1; j < patternSize; ++j) {
                        if (pattern[j] != -1 && pattern[j] != start[k + j]) {
                            found = false;
                            break;
                        }
                    }
                    if (found) {
                        results.push_back({ reinterpret_cast<uintptr_t>(start + k), true });
                        k += patternSize - 1; 
                    }
                 }
            }
        }
        return results;
    }

    PatternScanner PatternScanner::ScanRange(const uint8_t* start, size_t size, std::string_view signature) {
        return ScanInternal(start, size, ParseSignature(signature));
    }

    PatternScanner PatternScanner::FromAddress(uintptr_t address) {
        return { address, address != 0 };
    }
}