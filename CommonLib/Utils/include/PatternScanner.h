#pragma once
#include <cstdint>
#include <string_view>
#include <vector>
#include <optional>
#include <windows.h>

namespace Utils
{
    struct ScanResult
    {
        uintptr_t address = 0;
        bool found = false;

        operator bool() const { return found; }
        
        // Return a new result offset by value
        ScanResult Offset(intptr_t offset) const;

        // Dereference the pointer at the current address (or at an offset)
        // Equivalent to reading *(uintptr_t*)(address + offset)
        ScanResult Dereference(intptr_t offset = 0) const;

        // Calculate absolute address from a relative instruction (e.g., CALL or JMP)
        // instructionSize: The total size of the instruction (usually 5 for E8/E9)
        // offsetToDisplacement: Where the 4-byte displacement starts (usually 1 for E8/E9)
        ScanResult ResolveRelative(int instructionSize = 5, int offsetToDisplacement = 1) const;

        // Scan for another pattern relative to this address (The "Anchor" logic)
        // range > 0: Scans forward (increasing addresses)
        // range < 0: Scans backward (decreasing addresses)
        ScanResult ScanRelative(std::string_view signature, intptr_t range = 512) const;

        // Follows a multi-level pointer chain starting from this address.
        // Returns {0, false} if the chain is broken or memory is invalid.
        ScanResult ResolvePointerChain(const std::vector<int32_t>& offsets) const;

        // Scans backwards for function prologue (CC CC or 55 8B EC)
        ScanResult AlignToFunctionStart() const;

        template <typename T>
        T As() const { return (T)address; }
    };

    class PatternScanner
    {
    public:
        // Scan a specific module for a signature.
        // allSections: If false, only scans executable sections. If true, scans all readable sections.
        static ScanResult Scan(HMODULE module, std::string_view signature, bool allSections = false);

        // Scan the main module (GetModuleHandle(NULL))
        static ScanResult ScanMain(std::string_view signature, bool allSections = false);

        // Scan a specific module by name
        static ScanResult ScanModule(std::string_view moduleName, std::string_view signature, bool allSections = false);
        
        // Scan a specific section of the main module
        static ScanResult ScanSection(const char* sectionName, std::string_view signature);

        // Try multiple signatures until one is found
        static ScanResult ScanCandidates(const std::vector<std::string_view>& signatures, bool allSections = false);

        // Scan for all occurrences of a signature
        static std::vector<ScanResult> ScanAll(HMODULE module, std::string_view signature, bool allSections = false);

        // Scan a memory range
        static ScanResult ScanRange(const uint8_t* start, size_t size, std::string_view signature);

    private:
        friend struct ScanResult;
        static std::vector<int> ParseSignature(std::string_view signature);
        static ScanResult ScanInternal(const uint8_t* start, size_t size, const std::vector<int>& pattern);
    };
}