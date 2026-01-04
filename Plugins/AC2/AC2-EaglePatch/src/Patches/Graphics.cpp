#include "EaglePatch.h"
#include "Graphics.h"
#include <AutoAssemblerKinda.h>
#include <PatternScanner.h>
#include <memory>
#include "log.h"

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

    // --- Hook Wrappers ---

    DEFINE_HOOK(Cloth_Hook, Cloth_Return)
    {
        __asm {
            pop eax
            xor eax, eax
            jmp [Cloth_Return]
        }
    }

    DEFINE_HOOK(ForceLod0_Hook, ForceLod0_Return)
    {
        __asm {
            mov dword ptr [ebp-0x10], 0
            xor eax, eax
            jmp [ForceLod0_Return]
        }
    }

    DEFINE_HOOK(CheckIsCharacter_Hook, CheckIsCharacter_Return)
    {
        __asm {
            pushad
            pushfd
            
            mov edx, [ebp + 0x08]
            mov edx, [edx + 0x84]
            test edx, edx
            jz exit_hook

            mov ecx, [edx + 0x60]
            shr ecx, 0x12
            test cl, 1
            jz exit_hook

            // Entity is character, force LOD0
            mov dword ptr [esp + 0x20], 0 // eax in pushad (at esp+4+1C)
            mov dword ptr [ebp - 0x10], 0

        exit_hook:
            popfd
            popad

            // Stolen bytes
            mov cl, [ebx + 0x28]
            mov edi, [ebx + eax * 4 + 0x14]

            jmp [CheckIsCharacter_Return]
        }
    }

    namespace
    {
        bool ResolveAddresses(uintptr_t baseAddr, GameVersion version)
        {
            // ShadowMap: C7 41 20 00 04 00 00 5D
            auto shadowMap = Utils::PatternScanner::ScanModule("AssassinsCreedIIGame.exe", "C7 41 20 00 04 00 00 5D");
            if (!shadowMap) return false;
            sAddresses::ShadowMap = shadowMap.address + 0x03;

            // Cloth: E8 28 F4 FF FF 85
            auto cloth = Utils::PatternScanner::ScanModule("AssassinsCreedIIGame.exe", "E8 28 F4 FF FF 85");
            if (!cloth) return false;
            sAddresses::Cloth = cloth.address;
            Cloth_Return = cloth.address + 0x05;

            // LodLevel: 90 0F BE D1 F3 0F 10 04 95 ?? ?? 07 02
            auto lodLevel = Utils::PatternScanner::ScanModule("AssassinsCreedIIGame.exe", "90 0F BE D1 F3 0F 10 04 95 ?? ?? 07 02");
            if (!lodLevel) return false;
            sAddresses::LodLevel = lodLevel.address;

            // ForceLod0: D9 44 C7 4C 8B 77 48
            auto forceLod0 = Utils::PatternScanner::ScanModule("AssassinsCreedIIGame.exe", "D9 44 C7 4C 8B 77 48");
            if (!forceLod0) return false;
            sAddresses::ForceLod0 = forceLod0.address;
            ForceLod0_Return = 0; // Set in struct because it jumps to another hook

            // CheckChar: 8A 4B 28 8B 7C 83 14
            auto checkChar = Utils::PatternScanner::ScanModule("AssassinsCreedIIGame.exe", "8A 4B 28 8B 7C 83 14");
            if (!checkChar) return false;
            sAddresses::CheckChar = checkChar.address;
            sAddresses::CheckCharOut = checkChar.address + 0x07;
            CheckIsCharacter_Return = sAddresses::CheckCharOut;

            return true;
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
        DrawDistancePatches(uintptr_t clothAddr) {
            PresetScript_InjectJump(clothAddr, (uintptr_t)&Cloth_Hook);
        }
    };

    struct DrawDistanceHooks : AutoAssemblerCodeHolder_Base
    {
        DrawDistanceHooks(uintptr_t forceLod0, uintptr_t checkChar, uintptr_t checkCharJumpOut, uintptr_t lodLevelCalc)
        {
            ForceLod0_Return = checkChar;
            PresetScript_InjectJump(forceLod0, (uintptr_t)&ForceLod0_Hook);

            CheckIsCharacter_Return = checkCharJumpOut;
            PresetScript_InjectJump(checkChar, (uintptr_t)&CheckIsCharacter_Hook, 0x07);

            // 3. Force Maximum LOD Level from Distance calculation
            PresetScript_InjectJump(lodLevelCalc, lodLevelCalc + 0x44);
        }
    };

    using ShadowMapWrapper = AutoAssembleWrapper<ShadowMapPatch>;
    using DrawDistancePatchesWrapper = AutoAssembleWrapper<DrawDistancePatches>;
    using DrawDistanceHooksWrapper = AutoAssembleWrapper<DrawDistanceHooks>;

    static std::unique_ptr<ShadowMapWrapper> s_ShadowPatch;
    static std::unique_ptr<DrawDistancePatchesWrapper> s_DDPatch;
    static std::unique_ptr<DrawDistanceHooksWrapper> s_DDHooks;

    void InitGraphics(uintptr_t baseAddr, GameVersion version, bool shadows, bool drawDistance)
    {
        if (!ResolveAddresses(baseAddr, version))
            return;

        // Shadow Map
        s_ShadowPatch = std::make_unique<ShadowMapWrapper>(sAddresses::ShadowMap);
        if (shadows) s_ShadowPatch->Activate();

        // Draw Distance
        s_DDPatch = std::make_unique<DrawDistancePatchesWrapper>(sAddresses::Cloth);
        s_DDHooks = std::make_unique<DrawDistanceHooksWrapper>(sAddresses::ForceLod0, sAddresses::CheckChar, sAddresses::CheckCharOut, sAddresses::LodLevel);
        
        if (drawDistance) {
            s_DDPatch->Activate();
            s_DDHooks->Activate();
        }

        LOG_INFO("[EaglePatch] Graphics fixes initialized.");
    }

    void SetShadowMapResolution(bool enable)
    {
        if (s_ShadowPatch) s_ShadowPatch->Toggle(enable);
    }

    void SetDrawDistance(bool enable)
    {
        if (s_DDPatch) s_DDPatch->Toggle(enable);
        if (s_DDHooks) s_DDHooks->Toggle(enable);
    }
}