#pragma once
#include <cstdint>
#include <string_view>
#include <vector>
#include <optional>
#include <windows.h>
#include <type_traits>

namespace AutoAssemblerKinda
{
    // Checks if memory is committed and readable
    bool IsSafeRead(const void* ptr, size_t size);

    class PatternScanner
    {
    public:
        // --- Data ---
        uintptr_t m_Address = 0;
        bool m_Found = false;

        // --- Constructors ---
        PatternScanner(uintptr_t addr = 0, bool found = false) : m_Address(addr), m_Found(found) {}

        // --- Operator Overloads ---
        operator bool() const { return m_Found; }   // if (scan) ...
        operator uintptr_t() const { return m_Address; } // uintptr_t addr = scan;
        
        template <typename T>
        T As() const {
            if constexpr (std::is_pointer_v<T>) {
                using Pointee = std::remove_pointer_t<T>;
                if constexpr (!std::is_void_v<Pointee> && !std::is_function_v<Pointee>) {
                    if (!m_Found || m_Address == 0 || !IsSafeRead((void*)m_Address, sizeof(Pointee)))
                        return nullptr;
                }
            }
            return (T)m_Address;
        }

        // --- Manipulation (Chainable) ---
        
        // Return a new scanner result offset by value
        PatternScanner Offset(intptr_t offset) const;

        // Dereference the pointer at the current address (or at an offset)
        // Equivalent to reading *(uintptr_t*)(address + offset)
        PatternScanner Dereference(intptr_t offset = 0) const;

        // Calculate absolute address from a relative instruction (e.g., CALL or JMP)
        // instructionSize: The total size of the instruction (usually 5 for E8/E9)
        // offsetToDisplacement: Where the 4-byte displacement starts (usually 1 for E8/E9)
        PatternScanner ResolveRelative(int instructionSize = 5, int offsetToDisplacement = 1) const;

        // Scan for another pattern relative to this address (The "Anchor" logic)
        // range > 0: Scans forward (increasing addresses)
        // range < 0: Scans backward (decreasing addresses)
        PatternScanner ScanRelative(std::string_view signature, intptr_t range = 512) const;

        // Follows a multi-level pointer chain starting from this address.
        // Logic: Addr = *(Addr + Offset). Returns {0, false} if broken.
        PatternScanner ResolvePointerChain(const std::vector<int32_t>& offsets) const;

        // Extracts an absolute address from an instruction operand (32-bit).
        // e.g. MOV EAX, [0x12345678] -> returns 0x12345678.
        // instructionOffset: Offset from the scanned address to the start of the instruction.
        PatternScanner ExtractAbsoluteAddress(intptr_t instructionOffset = 0) const;

        // Scans backwards for function prologue (CC CC or 55 8B EC)
        PatternScanner AlignToFunctionStart() const;

        // --- Factory Methods (Static Scanning) ---
        
        // Scan a specific module for a signature.
        static PatternScanner Scan(HMODULE module, std::string_view signature, bool allSections = false, bool requireUnique = false);

        // Scan the main module (GetModuleHandle(NULL))
        static PatternScanner ScanMain(std::string_view signature, bool allSections = false, bool requireUnique = false);

        // Scan a specific module by name
        static PatternScanner ScanModule(std::string_view moduleName, std::string_view signature, bool allSections = false, bool requireUnique = false);
        
        // Scan a specific section of the main module
        static PatternScanner ScanSection(const char* sectionName, std::string_view signature);

        // Try multiple signatures until one is found
        static PatternScanner ScanCandidates(const std::vector<std::string_view>& signatures, bool allSections = false);

        // Scan for all occurrences of a signature
        static std::vector<PatternScanner> ScanAll(HMODULE module, std::string_view signature, bool allSections = false);

        // Scan a memory range
        static PatternScanner ScanRange(const uint8_t* start, size_t size, std::string_view signature);

        // Creates a result from a raw address.
        static PatternScanner FromAddress(uintptr_t address);

    private:
        static std::vector<int> ParseSignature(std::string_view signature);
        static PatternScanner ScanInternal(const uint8_t* start, size_t size, const std::vector<int>& pattern);
    };
}