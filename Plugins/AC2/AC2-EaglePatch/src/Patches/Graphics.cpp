#include "EaglePatch.h"
#include "Graphics.h"
#include <AutoAssemblerKinda.h>
#include "log.h"

namespace AC2EaglePatch
{
    // ==========================================================================
    // Graphics Improvements
    // ==========================================================================
    // This file contains patches for:
    // 1. Shadow Map Resolution - Increases shadow quality from 1024 to 4096
    // 2. Draw Distance - Multiple hooks to improve LOD and cloth rendering
    //
    // The draw distance system has interdependent hooks:
    // - Cloth_Hook: Disables cloth fade-out at distance
    // - ForceLod0_Hook: Forces highest LOD, chains to CheckIsCharacter
    // - CheckIsCharacter_Hook: Preserves LOD0 for characters specifically
    // - LodLevel: Skips distance-based LOD calculation entirely
    // ==========================================================================

    // ==========================================================================
    // 1. SHADOW MAP RESOLUTION PATCH
    // ==========================================================================
    // Pattern finds: mov dword ptr [ecx+20h], 400h (sets shadow map size to 1024)
    // We patch the immediate value 0x400 (1024) to 0x1000 (4096)
    // Offset +3 skips the opcode (C7 41 20) to reach the immediate value

    DEFINE_DATA_PATCH(ShadowMap, "C7 41 20 00 04 00 00 5D", 3, (int32_t)4096);

    // ==========================================================================
    // 2. DRAW DISTANCE HOOKS
    // ==========================================================================

    // --- 2a. Cloth Rendering Hook ---
    // Disables cloth fade-out at distance by returning 0 from the check
    DEFINE_AOB_HOOK(ClothHook, "E8 28 F4 FF FF 85", 0, 5);

    HOOK_IMPL(ClothHook)
    {
        __asm {
            pop eax          // Clean up stack from the CALL we're replacing
            xor eax, eax     // Return 0 (disable cloth fade)
            jmp [ClothHook_Return]
        }
    }

    // --- 2b. Force LOD0 Hook ---
    // Chains to CheckIsCharacter hook (the return address IS CheckIsCharacter's address)
    // This ensures we force LOD0 for most entities
    DEFINE_AOB_HOOK(ForceLod0, "D9 44 C7 4C 8B 77 48", 0, 7);

    // Derive from CheckIsCharacter hook (same pattern) to avoid redundant scan
    DEFINE_ADDRESS(CheckCharAddr, "@CheckIsCharacter", 0, RAW, nullptr);

    HOOK_IMPL(ForceLod0)
    {
        __asm {
            mov dword ptr [ebp-0x10], 0  // Force LOD index to 0
            xor eax, eax                  // Clear EAX
            jmp [ForceLod0_Return]        // Jump to CheckIsCharacter hook
        }
    }


    // --- 2c. Check Is Character Hook ---
    // Preserves LOD0 specifically for character entities (prevents pop-in)
    DEFINE_AOB_HOOK(CheckIsCharacter, "8A 4B 28 8B 7C 83 14", 0, 7);

    HOOK_IMPL(CheckIsCharacter)
    {
        __asm {
            pushad
            pushfd
            
            // Check if entity has valid component data
            mov edx, [ebp + 0x08]
            mov edx, [edx + 0x84]
            test edx, edx
            jz exit_check

            // Check if entity is flagged as character (bit 18 of flags)
            mov ecx, [edx + 0x60]
            shr ecx, 0x12
            test cl, 1
            jz exit_check

            // Entity is character - force LOD0
            mov dword ptr [esp + 0x20], 0   // eax in pushad
            mov dword ptr [ebp - 0x10], 0   // LOD index

        exit_check:
            popfd
            popad

            // Execute stolen bytes (original code we overwrote)
            mov cl, [ebx + 0x28]
            mov edi, [ebx + eax * 4 + 0x14]

            jmp [CheckIsCharacter_Return]
        }
    }

    // --- 2d. LOD Level Distance Calculation Skip ---
    // This patches the LOD distance calculation to always skip to the end
    // We use a simple NOP approach - skip the calculation entirely
    // Pattern: 90 0F BE D1 ...  (NOP followed by MOVSX)
    // We inject a jump that skips +0x44 bytes (the entire calculation)
    DEFINE_ADDRESS(LodLevelSkipLoc, "90 0F BE D1 F3 0F 10 04 95 ?? ?? 07 02", 0, RAW, nullptr);

    // For this complex case, we keep a small manual wrapper
    struct LodLevelSkipPatch : AutoAssemblerCodeHolder_Base
    {
        LodLevelSkipPatch(uintptr_t addr) {
            // Jump from addr to addr+0x44 (skip the distance calc)
            PresetScript_InjectJump(addr, addr + 0x44);
        }
    };

    // ==========================================================================
    // STATE MANAGEMENT
    // ==========================================================================

    static AutoAssembleWrapper<LodLevelSkipPatch>* s_LodSkipPatch = nullptr;
    static bool s_ShadowsEnabled = false;
    static bool s_DrawDistanceEnabled = false;

    void InitGraphics(uintptr_t baseAddr, GameVersion version, bool shadows, bool drawDistance)
    {
        // --- Resolve Shadow Map ---
        if (!ShadowMap_Descriptor.IsResolved()) {
            return;
        }

        // --- Resolve Draw Distance Hooks ---
        if (!ClothHook_Descriptor.IsResolved() ||
            !ForceLod0_Descriptor.IsResolved() ||
            !CheckCharAddr_Desc.IsResolved() ||
            !CheckIsCharacter_Descriptor.IsResolved())
        {
            return;
        }

        // Chain ForceLod0 to CheckIsCharacter (ForceLod0 returns via CheckIsCharacter)
        ForceLod0_Return = CheckCharAddr_Desc.GetAddress();

        // Create LodLevel skip patch
        if (LodLevelSkipLoc_Desc.IsResolved()) {
            s_LodSkipPatch = new AutoAssembleWrapper<LodLevelSkipPatch>(LodLevelSkipLoc_Desc.GetAddress());
        }

        // --- Apply Initial State ---
        s_ShadowsEnabled = shadows;
        s_DrawDistanceEnabled = drawDistance;

        if (shadows) {
            HookManager::Install(&ShadowMap_Descriptor);
            LOG_INFO("[EaglePatch] Shadow Map resolution improved.");
        }

        if (drawDistance) {
            HookManager::Install(&ClothHook_Descriptor);
            HookManager::Install(&ForceLod0_Descriptor);
            HookManager::Install(&CheckIsCharacter_Descriptor);
            if (s_LodSkipPatch) s_LodSkipPatch->Activate();
            LOG_INFO("[EaglePatch] Draw distance improvements applied.");
        }
    }

    void SetShadowMapResolution(bool enable)
    {
        if (enable) {
            HookManager::Install(&ShadowMap_Descriptor);
        } else {
            HookManager::Uninstall(&ShadowMap_Descriptor);
        }
        s_ShadowsEnabled = enable;
    }

    void SetDrawDistance(bool enable)
    {
        if (enable) {
            HookManager::Install(&ClothHook_Descriptor);
            HookManager::Install(&ForceLod0_Descriptor);
            HookManager::Install(&CheckIsCharacter_Descriptor);
            if (s_LodSkipPatch) s_LodSkipPatch->Activate();
        } else {
            HookManager::Uninstall(&ClothHook_Descriptor);
            HookManager::Uninstall(&ForceLod0_Descriptor);
            HookManager::Uninstall(&CheckIsCharacter_Descriptor);
            if (s_LodSkipPatch) s_LodSkipPatch->Deactivate();
        }
        s_DrawDistanceEnabled = enable;
    }
}