#include "../EaglePatch.h"
#include "Graphics.h"
#include <AutoAssemblerKinda.h>

namespace AC2EaglePatch
{
    // --- Shadow Map Patch ---
    struct ShadowMapPatch : AutoAssemblerCodeHolder_Base
    {
        ShadowMapPatch(uintptr_t addr) {
            DEFINE_ADDR(target, addr);
            // Overwrite 1024 (0x400) with 4096 (0x1000)
            target = { dd(4096) };
        }
    };

    // --- Draw Distance Patches ---
    struct DrawDistancePatches : AutoAssemblerCodeHolder_Base
    {
        // _forceLod0_cloth
        DrawDistancePatches(uintptr_t clothAddr) {
            DEFINE_ADDR(cloth, clothAddr);
            // Patch: pop eax; xor eax, eax; nop; nop
            cloth = { db({0x58, 0x31, 0xC0, 0x90, 0x90}) };
        }
    };

    // _AddHWGraphicObjectInstances_forceLod0 hook
    void Hook_ForceLod0(AllRegisters* params)
    {
        // Force lod0
        // mov [ebp-10h], 0
        *(uint32_t*)(params->ebp - 0x10) = 0;
        // xor eax, eax
        params->eax = 0;
    }

    // _AddHWGraphicObjectInstances_checkIsCharacter hook
    void Hook_CheckIsCharacter(AllRegisters* params)
    {
        // mov edx, [ebp+8]
        uint32_t arg1 = *(uint32_t*)(params->ebp + 8);
        // mov edx, [edx+84h]
        uint32_t entityPtr = *(uint32_t*)(arg1 + 0x84);

        if (entityPtr)
        {
            // mov ecx, [edx+60h]
            uint32_t flags = *(uint32_t*)(entityPtr + 0x60);
            // shr ecx, 12h; test cl, 1
            if ((flags >> 0x12) & 1)
            {
                // It is a character, force LOD0
                params->eax = 0;
                *(uint32_t*)(params->ebp - 0x10) = 0;
            }
        }

        // Original code we replaced (stolen bytes logic)
        // mov cl, [ebx+28h]
        // mov edi, [ebx+eax*4+14h]
        
        uint32_t ebx = params->ebx;
        uint32_t eax = params->eax;
        
        uint8_t cl = *(uint8_t*)(ebx + 0x28);
        params->ecx = (params->ecx & 0xFFFFFF00) | cl; // Set CL part of ECX
        
        params->edi = *(uint32_t*)(ebx + eax * 4 + 0x14);
    }

    struct DrawDistanceHooks : AutoAssemblerCodeHolder_Base
    {
        DrawDistanceHooks(uintptr_t forceLod0, uintptr_t checkChar, uintptr_t checkCharJumpOut, uintptr_t lodLevelCalc)
        {
            // 1. Force LOD0 Hook
            // Stolen bytes: 5 bytes (usually calls/instructions)
            // But here we want to inject logic.
            // The original code was likely customized in the manual assembly version.
            // Let's see: In manual version:
            // inst_force = { db(0xE9), RIP(inst_cave) };
            // inst_cave = { "31 C0", "89 45 F0", db(0xE9), RIP(check_char) };
            // Original instruction at forceLod0_inst_addr is likely what we overwrite.
            // Wait, we are overwriting `forceLod0` address.
            // `PresetScript_CCodeInTheMiddle` will replace 5 bytes at `forceLod0` with a call to our wrapper.
            // We pass `checkChar` as `whereToReturn`? 
            // In manual version: `jmp check_char`.
            // So we want to return to `checkChar`.
            // `Hook_ForceLod0` does the logic.
            // `executeStolenBytes` = false because we are replacing logic entirely/manually handling it?
            // Actually, the manual version didn't execute original bytes at `forceLod0`. It replaced them with logic then jumped to `check_char`.
            // So executeStolenBytes = false.
            
            PresetScript_CCodeInTheMiddle(forceLod0, 5, Hook_ForceLod0, checkChar, false);

            // 2. Check Is Character Hook
            // Manual version:
            // check_char = { db(0xE9), RIP(char_cave) };
            // char_cave logic... then `jmp check_char_out`.
            // We want to return to `checkCharJumpOut`.
            PresetScript_CCodeInTheMiddle(checkChar, 5, Hook_CheckIsCharacter, checkCharJumpOut, false);

            // 3. LOD Level from Distance (PatchJump)
            // PatchJump(sAddresses::_GetLODLevelFromDistance_forceMaxLod, sAddresses::_GetLODLevelFromDistance_forceMaxLod + 0x44);
            DEFINE_ADDR(lod_src, lodLevelCalc);
            DEFINE_ADDR(lod_dst, lodLevelCalc + 0x44);
            lod_src = { db(0xE9), RIP(lod_dst) };
        }
    };

    void InitGraphics(uintptr_t baseAddr, GameVersion version, bool shadows, bool drawDistance)
    {
        uintptr_t shadowMapAddr = 0;
        uintptr_t clothAddr = 0, lodLevelAddr = 0, forceLod0Addr = 0, checkCharAddr = 0, checkCharOutAddr = 0;

        switch (version)
        {
        case GameVersion::Version1: // Uplay
            shadowMapAddr = baseAddr + 0x117CD73 + 3; // +3 to skip opcode
            clothAddr = baseAddr + 0x11BFBB3;
            lodLevelAddr = baseAddr + 0x11FB1F;
            forceLod0Addr = baseAddr + 0x11BFCFC;
            checkCharAddr = baseAddr + 0x11BFE8F;
            checkCharOutAddr = baseAddr + 0x11BFE96;
            break;

        case GameVersion::Version2: // Retail 1.01
            shadowMapAddr = baseAddr + 0x6B9C13 + 3;
            clothAddr = baseAddr + 0x6FCE43;
            lodLevelAddr = baseAddr + 0x1495AF;
            forceLod0Addr = baseAddr + 0x6FCF8C;
            checkCharAddr = baseAddr + 0x6FD11F;
            checkCharOutAddr = baseAddr + 0x6FD126;
            break;

        default: return;
        }

        // Shadow Map
        if (shadows) {
            static AutoAssembleWrapper<ShadowMapPatch> shadowPatch(shadowMapAddr);
            shadowPatch.Activate();
        }

        // Draw Distance
        if (drawDistance) {
            static AutoAssembleWrapper<DrawDistancePatches> ddPatch(clothAddr);
            ddPatch.Activate();

            static AutoAssembleWrapper<DrawDistanceHooks> ddHooks(forceLod0Addr, checkCharAddr, checkCharOutAddr, lodLevelAddr);
            ddHooks.Activate();
        }

        if (g_loader_ref) g_loader_ref->LogToConsole("[EaglePatch] Graphics fixes applied.");
    }
}