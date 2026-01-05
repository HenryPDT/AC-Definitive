#include "Graphics.h"
#include <AutoAssemblerKinda.h>
#include <PatternScanner.h>

#include <dxgi.h>
#include <vector>
#include <cstring>
#include "log.h"

using namespace AutoAssemblerKinda;

namespace AC1EaglePatch
{
    // Helpers
    struct Nop3 { uint8_t v[3] = { 0x90, 0x90, 0x90 }; };

    // --- MSAA Patches ---

    // MSAA_Patch1: JMP short (0xEB) at offset +6
    // Pattern: 3B 81 84 00 00 00 72 17 E8
    DEFINE_DATA_PATCH(MSAA_Patch1, "3B 81 84 00 00 00 72 17 E8", 6, (uint8_t)0xEB);

    // MSAA_Hook (Patch2): Replaces comparison with "mov ecx, 1"
    // Pattern: 3B 8A 84 00 00 00
    // "3B 8A 84 00 00 00"
    DEFINE_AOB_HOOK(MSAA_Hook, "3B 8A 84 00 00 00", 0, 6);
    DEFINE_EXITS(MSAA_Hook, Exit, 6);

    HOOK_IMPL(MSAA_Hook)
    {
        __asm {
            mov ecx, 1
            jmp [MSAA_Hook_Exit]
        }
    }

    // MSAA_Patch3: NOPs at offset +0x0B
    // Pattern: Same as MSAA_Hook (relative)
    DEFINE_DATA_PATCH(MSAA_Patch3, "3B 8A 84 00 00 00", 0x0B, Nop3{});


    // --- DX10 Patches ---
    
    // Pattern for DX10 Checks & Functions: 6A 01 89 06 8B 08
    DEFINE_ADDRESS(DX10_Base, "6A 01 89 06 8B 08", 0, RAW, nullptr);

    // Check1 at +1, Check2 at +0x43. Patch with 0x00.
    DEFINE_DATA_PATCH(DX10_Patch1, "6A 01 89 06 8B 08", 1, (uint8_t)0x00);
    DEFINE_DATA_PATCH(DX10_Patch2, "6A 01 89 06 8B 08", 0x43, (uint8_t)0x00);

    struct D3D10ResolutionContainer;
    // Pointers to game functions
    using t_GetDisplayModes = void(__thiscall*)(D3D10ResolutionContainer*, IDXGIOutput*);
    using t_FindCurrentResolutionMode = void(__thiscall*)(D3D10ResolutionContainer*, uint32_t, uint32_t, uint32_t);

    t_GetDisplayModes fnGetDisplayModes = nullptr;
    t_FindCurrentResolutionMode fnFindCurrentResolutionMode = nullptr;

    // fnGetDisplayModes at -0x0E (Address of function)
    DEFINE_ADDRESS(GetDisplayModesAddr, "@DX10_Base", -0x0E, RAW, (uintptr_t*)&fnGetDisplayModes);

    // fnFindCurrentResolutionMode at +0x5A (Relative Call)
    DEFINE_ADDRESS(FindCurrentResAddr, "@DX10_Base", 0x5A, CALL, (uintptr_t*)&fnFindCurrentResolutionMode);

    // Forward declaration
    void __fastcall Hook_GetDisplayModes(D3D10ResolutionContainer* thisPtr, void* /*edx*/, IDXGIOutput* a1);

    // DX10_Hook
    // Pattern: E8 DE 78 FC FF
    DEFINE_AOB_HOOK(DX10_Hook, "E8 DE 78 FC FF", 0, 5);
    DEFINE_EXITS(DX10_Hook, Exit, 5);

    HOOK_IMPL(DX10_Hook)
    {
        __asm {
            call Hook_GetDisplayModes
            jmp [DX10_Hook_Exit]
        }
    }

    struct D3D10ResolutionContainer
    {
        IDXGIOutput* DXGIOutput;
        DXGI_MODE_DESC* modes;
        uint32_t m_width, m_height, m_refreshRate, _5, _6;
        uint32_t modesNum;
        uint32_t _8;

        void GetDisplayModes(IDXGIOutput* a1) { if (fnGetDisplayModes) fnGetDisplayModes(this, a1); }
        void FindCurrentResolutionMode(uint32_t w, uint32_t h, uint32_t r) { if (fnFindCurrentResolutionMode) fnFindCurrentResolutionMode(this, w, h, r); }
    };

    namespace
    {
        bool IsDisplayModeAlreadyAdded(const DXGI_MODE_DESC& mode, const std::vector<DXGI_MODE_DESC>& newModes)
        {
            for (const auto& existing : newModes)
            {
                if (existing.Width == mode.Width && existing.Height == mode.Height
                    && existing.RefreshRate.Numerator == mode.RefreshRate.Numerator
                    && existing.Format == mode.Format && existing.ScanlineOrdering == mode.ScanlineOrdering)
                    return true;
            }
            return false;
        }
    }

    // This function will be called from the hook
    void __fastcall Hook_GetDisplayModes(D3D10ResolutionContainer* thisPtr, void* /*edx*/, IDXGIOutput* a1)
    {
        if (!thisPtr) return;

        thisPtr->GetDisplayModes(a1); // Call original to populate list

        std::vector<DXGI_MODE_DESC> newModes;
        newModes.reserve(thisPtr->modesNum);

        if (thisPtr->modes && thisPtr->modesNum > 0)
        {
            for (uint32_t i = 0; i < thisPtr->modesNum; i++)
            {
                if (!IsDisplayModeAlreadyAdded(thisPtr->modes[i], newModes))
                {
                    newModes.push_back(thisPtr->modes[i]);
                }
            }

            // Overwrite original array with filtered list
            memset(thisPtr->modes, 0, sizeof(DXGI_MODE_DESC) * thisPtr->modesNum); // Clear
            if (!newModes.empty())
            {
                memcpy(thisPtr->modes, newModes.data(), sizeof(DXGI_MODE_DESC) * newModes.size());
            }
            thisPtr->modesNum = (uint32_t)newModes.size();
        }

        thisPtr->FindCurrentResolutionMode(thisPtr->m_width, thisPtr->m_height, thisPtr->m_refreshRate);
    }
    
    void InitGraphics(uintptr_t baseAddr, GameVersion version, bool enableMSAAFix, bool fixDX10Resolution)
    {
        // 1. MSAA Fix
        if (enableMSAAFix)
        {
            HookManager::Install(&MSAA_Patch1_Descriptor);
            HookManager::Install(&MSAA_Hook_Descriptor);
            HookManager::Install(&MSAA_Patch3_Descriptor);
        }

        // 2. DX10 Resolution Fix
        if (fixDX10Resolution && version == GameVersion::Version1)
        {
            HookManager::Install(&DX10_Patch1_Descriptor);
            HookManager::Install(&DX10_Patch2_Descriptor);
            HookManager::Install(&DX10_Hook_Descriptor);
            LOG_INFO("[EaglePatch] Graphics fixes applied (DX10).");
        }
    }
}
