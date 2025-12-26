#include "../EaglePatch.h"
#include "Graphics.h"
#include <AutoAssemblerKinda.h>

namespace AC2EaglePatch
{
    struct sAddresses {
        static uintptr_t ShadowMap;
        static uintptr_t Cloth;
        static uintptr_t LodLevel;
        static uintptr_t ForceLod0;
        static uintptr_t CheckChar;
        static uintptr_t CheckCharOut;
    };

    uintptr_t sAddresses::ShadowMap = 0;
    uintptr_t sAddresses::Cloth = 0;
    uintptr_t sAddresses::LodLevel = 0;
    uintptr_t sAddresses::ForceLod0 = 0;
    uintptr_t sAddresses::CheckChar = 0;
    uintptr_t sAddresses::CheckCharOut = 0;

    namespace
    {
        bool ResolveAddresses(uintptr_t baseAddr, GameVersion version)
        {
            switch (version)
            {
            case GameVersion::Version1: // Uplay
                sAddresses::ShadowMap = baseAddr + 0x117CD73 + 3; // +3 to skip opcode
                sAddresses::Cloth = baseAddr + 0x11BFBB3;
                sAddresses::LodLevel = baseAddr + 0x11FB1F;
                sAddresses::ForceLod0 = baseAddr + 0x11BFCFC;
                sAddresses::CheckChar = baseAddr + 0x11BFE8F;
                sAddresses::CheckCharOut = baseAddr + 0x11BFE96;
                return true;
            case GameVersion::Version2: // Retail 1.01
                sAddresses::ShadowMap = baseAddr + 0x6B9C13 + 3;
                sAddresses::Cloth = baseAddr + 0x6FCE43;
                sAddresses::LodLevel = baseAddr + 0x1495AF;
                sAddresses::ForceLod0 = baseAddr + 0x6FCF8C;
                sAddresses::CheckChar = baseAddr + 0x6FD11F;
                sAddresses::CheckCharOut = baseAddr + 0x6FD126;
                return true;
            default:
                return false;
            }
        }
    }

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
        // Force LOD0 for specific objects (Cloth/Characters)
        // mov [ebp-10h], 0  ; Set local variable to 0
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
            // Check entity flags to identify characters
            // mov ecx, [edx+60h]
            uint32_t flags = *(uint32_t*)(entityPtr + 0x60);
            // shr ecx, 12h; test cl, 1
            if ((flags >> 0x12) & 1)
            {
                // Entity is a character, force LOD0
                params->eax = 0;
                *(uint32_t*)(params->ebp - 0x10) = 0;
            }
        }

        // Execute original instructions (stolen bytes simulation)
        // mov cl, [ebx+28h] (Note: Only updating CL part of ECX)
        uint32_t ebx = params->ebx;
        uint32_t eax = params->eax;
        uint8_t cl = *(uint8_t*)(ebx + 0x28);
        params->ecx = (params->ecx & 0xFFFFFF00) | cl;
        
        // mov edi, [ebx+eax*4+14h]
        params->edi = *(uint32_t*)(ebx + eax * 4 + 0x14);
    }

    struct DrawDistanceHooks : AutoAssemblerCodeHolder_Base
    {
        DrawDistanceHooks(uintptr_t forceLod0, uintptr_t checkChar, uintptr_t checkCharJumpOut, uintptr_t lodLevelCalc)
        {
            // 1. Hook ForceLod0 -> Jumps to CheckIsCharacter logic
            // We skip original bytes and custom-route the flow to the character check
            PresetScript_CCodeInTheMiddle(forceLod0, 5, Hook_ForceLod0, checkChar, false);

            // 2. Hook CheckIsCharacter -> Returns to main flow (checkCharJumpOut)
            PresetScript_CCodeInTheMiddle(checkChar, 5, Hook_CheckIsCharacter, checkCharJumpOut, false);

            // 3. Force Maximum LOD Level from Distance calculation
            // Jumps over the distance check to always return high quality LOD
            DEFINE_ADDR(lod_src, lodLevelCalc);
            DEFINE_ADDR(lod_dst, lodLevelCalc + 0x44);
            lod_src = { db(0xE9), RIP(lod_dst) };
        }
    };

    void InitGraphics(uintptr_t baseAddr, GameVersion version, bool shadows, bool drawDistance)
    {
        if (!ResolveAddresses(baseAddr, version))
            return;

        // Shadow Map
        if (shadows) {
            static AutoAssembleWrapper<ShadowMapPatch> shadowPatch(sAddresses::ShadowMap);
            shadowPatch.Activate();
        }

        // Draw Distance
        if (drawDistance) {
            static AutoAssembleWrapper<DrawDistancePatches> ddPatch(sAddresses::Cloth);
            ddPatch.Activate();

            static AutoAssembleWrapper<DrawDistanceHooks> ddHooks(sAddresses::ForceLod0, sAddresses::CheckChar, sAddresses::CheckCharOut, sAddresses::LodLevel);
            ddHooks.Activate();
        }

        if (g_loader_ref) g_loader_ref->LogToConsole("[EaglePatch] Graphics fixes applied.");
    }
}