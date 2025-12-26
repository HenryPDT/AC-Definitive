#include "Graphics.h"
#include <AutoAssemblerKinda.h>
#include <dxgi.h>
#include <vector>
#include <cstring>

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

    namespace
    {
        bool ResolveAddresses(uintptr_t baseAddr, GameVersion version)
        {
            switch (version)
            {
            case GameVersion::Version1: // DX10
                sAddresses::MSAA_1 = baseAddr + 0xC64252;
                sAddresses::MSAA_2 = baseAddr + 0xC63F9D;
                sAddresses::MSAA_3 = baseAddr + 0xC63FA8;
                sAddresses::DX10_Check1 = baseAddr + 0x3BAD2E + 1;
                sAddresses::DX10_Check2 = baseAddr + 0x3BAD70 + 1;
                sAddresses::DX10_Hook   = baseAddr + 0x3F343D;
                fnGetDisplayModes = (t_GetDisplayModes)(baseAddr + 0x3BAD20);
                fnFindCurrentResolutionMode = (t_FindCurrentResolutionMode)(baseAddr + 0x3BA770);
                return true;
            case GameVersion::Version2: // DX9
                sAddresses::MSAA_1 = baseAddr + 0xA91422;
                sAddresses::MSAA_2 = baseAddr + 0xA9116D;
                sAddresses::MSAA_3 = baseAddr + 0xA91178;
                return true;
            default:
                return false;
            }
        }
    }

    // --- Multisampling Patch ---
    struct MultisamplingPatch : AutoAssemblerCodeHolder_Base
    {
        MultisamplingPatch(uintptr_t addr1, uintptr_t addr2, uintptr_t addr3) {
            DEFINE_ADDR(ms1, addr1);
            DEFINE_ADDR(ms2, addr2);
            DEFINE_ADDR(ms3, addr3);

            // Patch 1: JMP short (0xEB)
            ms1 = { db(0xEB) };

            // Patch 2: MOV ECX, 1; NOP (B9 01 00 00 00 90)
            ms2 = { db({0xB9, 0x01, 0x00, 0x00, 0x00, 0x90}) };

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
            DEFINE_ADDR(hook_loc, hookAddress);
            DEFINE_ADDR(retAddr, hookAddress + 5); // Return after the original call (CALL rel32 is 5 bytes)

            ALLOC(cave, 64, hookAddress);
            LABEL(fnVar);

            // Patch the call site with a JMP to cave
            hook_loc = { db(0xE9), RIP(cave) };

            // Cave implementation:
            // The original instruction was a `call MemberFunc` (thiscall).
            // We redirect to our static __fastcall function `Hook_GetDisplayModes`.
            // __fastcall (ECX, EDX, Stack) matches the register state of `thiscall` (ECX, Stack)
            // regarding ECX ('this') and the Stack args, effectively treating EDX as a dummy arg.
            
            cave = {
                "FF 15", ABS(fnVar, 4), // Call dword ptr [fnVar]
                "E9", RIP(retAddr),     // Jmp back

                // Define the pointer variable inline
                PutLabel(fnVar),
                dd((uint32_t)Hook_GetDisplayModes)
            };
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