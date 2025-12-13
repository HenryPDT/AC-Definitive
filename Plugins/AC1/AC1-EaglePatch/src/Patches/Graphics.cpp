#include "Graphics.h"
#include <AutoAssemblerKinda.h>
#include <dxgi.h>
#include <vector>
#include <cstring>

namespace AC1EaglePatch
{
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

        // Function pointers need to be set at runtime
        static inline void(__thiscall* _GetDisplayModes)(D3D10ResolutionContainer*, IDXGIOutput*) = nullptr;
        static inline void(__thiscall* _FindCurrentResolutionMode)(D3D10ResolutionContainer*, uint32_t, uint32_t, uint32_t) = nullptr;

        void GetDisplayModes(IDXGIOutput* a1) { if (_GetDisplayModes) _GetDisplayModes(this, a1); }
        void FindCurrentResolutionMode(uint32_t w, uint32_t h, uint32_t r) { if (_FindCurrentResolutionMode) _FindCurrentResolutionMode(this, w, h, r); }
    };

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

            // Cave implementation
            // We are hooking a CALL instruction: call RelativeAddr
            // The original function expects ECX (this) and 1 argument on stack.
            // Our Hook_GetDisplayModes is __fastcall: ECX = this, EDX = unused, Stack = args.
            // However, the original call pushes arg then calls.
            // So:
            // Stack at entry to cave: [RetAddr of caller] [Arg] ...
            // We want to call Hook_GetDisplayModes(thisPtr, edx, arg)
            // __fastcall passes first 2 args in ECX, EDX.
            // ECX is already set by caller (it's a method call).
            // We can just call our function.
            // BUT: Hook_GetDisplayModes signature matches what the wrapper needs.
            // __fastcall: ECX=this, EDX=dummy. Arg is on stack.
            // This matches cleanly if we just CALL it, provided we clean up or match calling convention.
            // Actually, we are replacing a `call MemberFunc`.
            // We can just call our static function if we ensure register state is correct.
            
            // Wait, Hook_GetDisplayModes is `void __fastcall(D3D10ResolutionContainer* thisPtr, void* edx, IDXGIOutput* a1)`
            // Calling convention __fastcall:
            // ECX = 1st arg (thisPtr) -> Matches
            // EDX = 2nd arg (void* edx) -> Garbage/Unused -> Matches
            // Stack = 3rd arg (IDXGIOutput* a1) -> Matches (pushed by caller)
            // So we can just call it directly!
            // BUT: The original code does `call MemberFunc`. 
            // `MemberFunc` is `thiscall`.
            // `thiscall` (MSVC): ECX = this, rest on stack. Callee cleans stack.
            // `fastcall` (MSVC): ECX, EDX, rest on stack. Callee cleans stack.
            //
            // Our Hook function takes (thisPtr, dummy, a1).
            // ECX = thisPtr.
            // EDX = dummy.
            // a1 is on stack.
            //
            // If we define Hook_GetDisplayModes as __fastcall, it expects to clean up `a1` from stack (ret 4).
            // The caller pushed `a1`.
            // So this works perfectly.
            
            cave = {
                "FF 15", ABS(fnVar, 4), // Call dword ptr [fnVar]
                "E9", RIP(retAddr),     // Jmp back

                // Define the pointer variable inline
                PutLabel(fnVar),
                dd((uint32_t)Hook_GetDisplayModes)
            };
        }
    };

    void InitGraphics(uintptr_t baseAddr, GameVersion version)
    {
        // Multisampling Addresses
        uintptr_t ms1 = 0, ms2 = 0, ms3 = 0;
        
        // DX10 Fix Addresses
        uintptr_t dx10_check1 = 0, dx10_check2 = 0, dx10_hook = 0;
        bool isDX10 = false;

        switch (version)
        {
        case GameVersion::Version1: // DX10
            ms1 = baseAddr + 0xC64252;
            ms2 = baseAddr + 0xC63F9D;
            ms3 = baseAddr + 0xC63FA8;

            dx10_check1 = baseAddr + 0x3BAD2E + 1;
            dx10_check2 = baseAddr + 0x3BAD70 + 1;
            dx10_hook   = baseAddr + 0x3F343D;

            // Setup function pointers for DX10 fix
            D3D10ResolutionContainer::_GetDisplayModes = (void(__thiscall*)(D3D10ResolutionContainer*, IDXGIOutput*))(baseAddr + 0x3BAD20);
            D3D10ResolutionContainer::_FindCurrentResolutionMode = (void(__thiscall*)(D3D10ResolutionContainer*, uint32_t, uint32_t, uint32_t))(baseAddr + 0x3BA770);
            isDX10 = true;
            break;

        case GameVersion::Version2: // DX9
            ms1 = baseAddr + 0xA91422;
            ms2 = baseAddr + 0xA9116D;
            ms3 = baseAddr + 0xA91178;
            break;

        default: return;
        }

        // Apply Multisampling Patch
        static AutoAssembleWrapper<MultisamplingPatch> msPatch(ms1, ms2, ms3);
        msPatch.Activate();

        if (isDX10)
        {
            // Apply Duplicate Resolution Fix
            static AutoAssembleWrapper<DX10ResolutionPatch> resPatch(dx10_check1, dx10_check2);
            resPatch.Activate();

            static AutoAssembleWrapper<DX10GetDisplayModesHook> hookPatch(dx10_hook);
            hookPatch.Activate();

            if (g_loader_ref) g_loader_ref->LogToConsole("[EaglePatch] Graphics fixes applied (DX10).");
        }
        else
        {
            if (g_loader_ref) g_loader_ref->LogToConsole("[EaglePatch] Graphics fixes applied (DX9).");
        }
    }
}