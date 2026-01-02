#include "Graphics.h"
#include <AutoAssemblerKinda.h>
#include <PatternScanner.h>

#include <dxgi.h>
#include <vector>
#include <cstring>

using namespace Utils;

namespace AC1EaglePatch
{
    struct sAddresses {
        static uintptr_t MSAA_1;
        static uintptr_t MSAA_2;
        static uintptr_t MSAA_3;
        static uintptr_t DX10_Check1;
        static uintptr_t DX10_Check2;
        static uintptr_t DX10_Hook;
    };

    uintptr_t sAddresses::MSAA_1 = 0;
    uintptr_t sAddresses::MSAA_2 = 0;
    uintptr_t sAddresses::MSAA_3 = 0;
    uintptr_t sAddresses::DX10_Check1 = 0;
    uintptr_t sAddresses::DX10_Check2 = 0;
    uintptr_t sAddresses::DX10_Hook = 0;

    struct D3D10ResolutionContainer;
    // Pointers to game functions
    using t_GetDisplayModes = void(__thiscall*)(D3D10ResolutionContainer*, IDXGIOutput*);
    using t_FindCurrentResolutionMode = void(__thiscall*)(D3D10ResolutionContainer*, uint32_t, uint32_t, uint32_t);

    t_GetDisplayModes fnGetDisplayModes = nullptr;
    t_FindCurrentResolutionMode fnFindCurrentResolutionMode = nullptr;

    // Hook Wrappers
    DEFINE_HOOK(MSAA_Patch2_Wrapper, MSAA_Patch2_Return)
    {
        __asm {
            mov ecx, 1
            jmp [MSAA_Patch2_Return]
        }
    }
    
    // Forward declaration
    void __fastcall Hook_GetDisplayModes(D3D10ResolutionContainer* thisPtr, void* /*edx*/, IDXGIOutput* a1);

    DEFINE_HOOK(Hook_GetDisplayModes_Wrapper, DX10_Hook_Return)
    {
        __asm {
            call Hook_GetDisplayModes
            jmp [DX10_Hook_Return]
        }
    }

    namespace
    {
        bool ResolveAddresses(uintptr_t baseAddr, GameVersion version)
        {
            // MSAA Patches
            
            // MSAA_1: 3B 81 84 00 00 00 72 17
            // Target is at 72 17 (+6 from match)
            auto msaa1 = PatternScanner::ScanMain("3B 81 84 00 00 00 72 17 E8");
            if (msaa1) sAddresses::MSAA_1 = msaa1.Offset(6).address;

            // MSAA_2: 3B 8A 84 00 00 00
            auto msaa2 = PatternScanner::ScanMain("3B 8A 84 00 00 00");
            if (msaa2) {
                sAddresses::MSAA_2 = msaa2.address;
                sAddresses::MSAA_3 = msaa2.Offset(0xB).address;
                MSAA_Patch2_Return = msaa2.address + 6;
            }

            if (version == GameVersion::Version1) // DX10
            {
                // DX10 Checks & Functions: 6A 01 89 06 8B 08
                // Match at 3BAD2E
                auto check = PatternScanner::ScanMain("6A 01 89 06 8B 08");
                if (check) {
                    // DX10_Check1 (3BAD2E+1) is at +1
                    sAddresses::DX10_Check1 = check.Offset(1).address;
                    
                    // DX10_Check2 (3BAD70+1) is at +0x43 (3BAD70 - 3BAD2E = 0x42)
                    sAddresses::DX10_Check2 = check.Offset(0x43).address;

                    // fnGetDisplayModes (3BAD20) is at -0xE
                    fnGetDisplayModes = check.Offset(-0xE).As<t_GetDisplayModes>();

                    // fnFindCurrentResolutionMode is called at 3BAD88 (Match + 0x5A)
                    auto callSite = check.Offset(0x5A);
                    if (*callSite.As<uint8_t*>() == 0xE8) {
                        fnFindCurrentResolutionMode = (t_FindCurrentResolutionMode)callSite.ResolveRelative().address;
                    }
                }

                // DX10_Hook: E8 DE 78 FC FF
                // Match at 3F343D
                auto hook = PatternScanner::ScanMain("E8 DE 78 FC FF");
                if (hook) {
                    sAddresses::DX10_Hook = hook.address;
                    DX10_Hook_Return = hook.address + 5;
                }
            }

            if (version == GameVersion::Version1)
                return sAddresses::MSAA_1 && sAddresses::MSAA_2 && sAddresses::DX10_Check1 && sAddresses::DX10_Hook;
            else
                return sAddresses::MSAA_1 && sAddresses::MSAA_2;
        }
    }

    // --- Multisampling Patch ---
    struct MultisamplingPatch : AutoAssemblerCodeHolder_Base
    {
        MultisamplingPatch(uintptr_t addr1, uintptr_t addr2, uintptr_t addr3) {
            DEFINE_ADDR(ms1, addr1);
            DEFINE_ADDR(ms3, addr3);

            // Patch 1: JMP short (0xEB)
            ms1 = { db(0xEB) };

            // Patch 2: MOV ECX, 1; NOP (B9 01 00 00 00 90)
            // Replaced with InjectJump to MSAA_Patch2_Wrapper
            // Stolen bytes: 6 (cmp instruction). InjectJump (5) + NOP (1).
            PresetScript_InjectJump(addr2, (uintptr_t)&MSAA_Patch2_Wrapper, 6);

            // Patch 3: NOPs (3 bytes)
            ms3 = { nop(3) };
        }
    };

    // --- DX10 Duplicate Resolutions Fix ---
    struct DX10ResolutionPatch : AutoAssemblerCodeHolder_Base
    {
        DX10ResolutionPatch(uintptr_t interlacedCheck1, uintptr_t interlacedCheck2) {
            DEFINE_ADDR(chk1, interlacedCheck1);
            DEFINE_ADDR(chk2, interlacedCheck2);

            // Disable interlaced checks (set bytes to 0)
            chk1 = { db(0x00) };
            chk2 = { db(0x00) };
        }
    };

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
    
    struct DX10GetDisplayModesHook : AutoAssemblerCodeHolder_Base
    {
        DX10GetDisplayModesHook(uintptr_t hookAddress) {
            PresetScript_InjectJump(hookAddress, (uintptr_t)&Hook_GetDisplayModes_Wrapper);
        }
    };

    void InitGraphics(uintptr_t baseAddr, GameVersion version, bool enableMSAAFix, bool fixDX10Resolution)
    {
        if (!ResolveAddresses(baseAddr, version))
            return;

        if (enableMSAAFix)
        {
            // Apply Multisampling Patch
            static AutoAssembleWrapper<MultisamplingPatch> msPatch(sAddresses::MSAA_1, sAddresses::MSAA_2, sAddresses::MSAA_3);
            msPatch.Activate();
        }

        // Apply DX10 Duplicate Resolution Fix only if addresses were found (i.e., we are on DX10 version)
        if (fixDX10Resolution && sAddresses::DX10_Hook != 0)
        {
            static AutoAssembleWrapper<DX10ResolutionPatch> resPatch(sAddresses::DX10_Check1, sAddresses::DX10_Check2);
            resPatch.Activate();

            static AutoAssembleWrapper<DX10GetDisplayModesHook> hookPatch(sAddresses::DX10_Hook);
            hookPatch.Activate();

            if (g_loader_ref) g_loader_ref->LogToConsole("[EaglePatch] Graphics fixes applied (DX10).");
        }
    }
}
