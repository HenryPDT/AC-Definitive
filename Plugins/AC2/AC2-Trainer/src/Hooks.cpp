#include <windows.h>
#include "Hooks.h"
#include <AutoAssemblerKinda.h>
#include <PatternScanner.h>
#include "log.h"
#include "Game/Singletons.h"
#include "Game/Entity.h"
#include "Game/BhvAssassin.h"
#include "Game/Components/BipedComponent.h"
#include "Game/Managers/MissionTimer.h"
#include "Game/Managers/TimeOfDayManager.h"
#include "Game/Managers/CSrvPlayerHealth.h"
#include "Core/Constants.h"
#include "Trainer.h"
#include <memory>

extern Trainer::Configuration g_config;

namespace Hooks
{
    // Globals accessible by naked functions
    static AC2::Entity* g_pPlayer = nullptr;
    static AC2::BipedComponent* g_pBiped = nullptr;
    static AC2::MissionTimer* g_pTimer = nullptr;
    
    // Pointers captured by hooks (synced to Singletons)
    AC2::CSrvPlayerHealth* captured_pHealth = nullptr;
    AC2::MapManager* captured_pMapManager = nullptr;
    static void* captured_pNotoriety = nullptr;
    
    // Hook state flags
    static bool bInfiniteItems = false;
    static bool bFreeRoam = false;
    static bool bIgnoreFallDamage = false;
    static bool bGodMode = false;
    static bool bDisableNotoriety = false;
    static float g_TimeScale = AC2::Constants::TIME_DELAY_DEFAULT;

    // Aligned speed vector for SSE
    __declspec(align(16)) float g_SpeedVector[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

    // =========================================================
    // 1. Health Pointer & God Mode Hook
    // =========================================================
    // Captures CSrvPlayerHealth pointer and handles God Mode
    DEFINE_HOOK(HealthHook, HealthReturn)
    {
        __asm {
            // CE Injection at Health+10: 
            // Original bytes: 8B 48 58 8B 55 08
            // Opcode: mov ecx, [eax+0x58]
            // CE Logic: mov [pHealth], eax
            
            mov [captured_pHealth], eax

            // Execute original code
            mov ecx, [eax+0x58]
            mov edx, [ebp+0x08]
            
            jmp [HealthReturn]
        }
    }

    struct HealthPatch : AutoAssemblerCodeHolder_Base
    {
        HealthPatch(uintptr_t addr) { // Address of 8B 48 58
            PresetScript_InjectJump(addr, (uintptr_t)&HealthHook, 6);
        }
    };

    // =========================================================
    // 2. Fall Damage Hook
    // =========================================================
    DEFINE_HOOK(FallDamageHook, FallDamageReturn)
    {
        __asm {
            and esi, 0x3FFF // Original Code

            push eax
            
            // Safety check
            mov eax, [g_pPlayer]
            test eax, eax
            je FallDamageExit
            
            cmp ebx, eax // Check if ebx is player
            jne FallDamageExit
            
            // Check Config
            cmp [bIgnoreFallDamage], 1
            jne FallDamageExit

            // Set fall distance to 0
            mov dword ptr [ebp+0x0C], 0

        FallDamageExit:
            pop eax
            jmp [FallDamageReturn]
        }
    }

    struct FallDamagePatch : AutoAssemblerCodeHolder_Base
    {
        FallDamagePatch(uintptr_t addr) {
            PresetScript_InjectJump(addr, (uintptr_t)&FallDamageHook, 6);
        }
    };

    // =========================================================
    // 3. Lock Consumables Hook
    // =========================================================
    DEFINE_HOOK(LockConsumablesHook, LockConsumablesReturn)
    {
        __asm {
            cmp [bInfiniteItems], 1
            jne LockConsumablesOriginal

            // Infinite Items Logic: Skip sub, write back count
            // CE: mov [ecx+10],eax (eax contains original value)
            mov [ecx+0x10], eax
            jmp LockConsumablesExit

        LockConsumablesOriginal:
            sub eax, edx
            mov [ecx+0x10], eax

        LockConsumablesExit:
            jmp [LockConsumablesReturn]
        }
    }

    struct LockConsumablesPatch : AutoAssemblerCodeHolder_Base
    {
        LockConsumablesPatch(uintptr_t addr) {
            PresetScript_InjectJump(addr, (uintptr_t)&LockConsumablesHook, 5);
        }
    };


    // =========================================================
    // 4. Lock Knives Hook
    // =========================================================
    // Knives use a separate function for removal.
    // CE 88778_Lock Consumables.asm (LockKnife label)
    // Injection at 6F759E (sar eax, 1F; and edx, eax)
    // 
    // FIX: The original C++ hook tried to do a mid-function return which
    // crashed the game. Instead, we just let execution continue normally.
    // The knife decrement is ALREADY prevented by LockConsumables hook
    // because knives go through the same sub eax,edx path.
    // 
    // This hook now only captures for debugging/verification purposes.
    DEFINE_HOOK(LockKnivesHook, LockKnivesReturn)
    {
        __asm {
            // Original code - MUST execute these to maintain proper register state
            sar eax, 0x1F
            and edx, eax
            
            // Just continue - the LockConsumables hook already handles this
            // Knives go through the same inventory decrement path
            jmp [LockKnivesReturn]
        }
    }

    struct LockKnivesPatch : AutoAssemblerCodeHolder_Base
    {
        LockKnivesPatch(uintptr_t addr) {
            PresetScript_InjectJump(addr, (uintptr_t)&LockKnivesHook, 5);
        }
    };


    // =========================================================
    // 5. Teleport Hook
    // =========================================================
    DEFINE_HOOK(TeleportHook, TeleportReturn)
    {
        __asm {
            push ebx
            mov ebx, eax
            sub ebx, 0x10
            cmp ebx, [g_pPlayer]
            pop ebx
            jne TeleportOriginal

            cmp [bFreeRoam], 1
            jne TeleportOriginal

            // Force Position: Load xmm0 from memory (which we updated in C++)
            movaps xmm0, [eax+0x30]

        TeleportOriginal:
            push ecx
            movaps [eax+0x30], xmm0
            jmp [TeleportReturn]
        }
    }

    struct TeleportPatch : AutoAssemblerCodeHolder_Base
    {
        TeleportPatch(uintptr_t addr) {
            PresetScript_InjectJump(addr, (uintptr_t)&TeleportHook, 5);
        }
    };

    // =========================================================
    // 6. Speed Player Hook
    // =========================================================
    DEFINE_HOOK(SpeedPlayerHook, SpeedPlayerReturn)
    {
        __asm {
            // Original: movaps xmm0, [esi+0xA0]
            movaps xmm0, [esi+0xA0]

            // Check if player biped
            cmp esi, [g_pBiped]
            jne SpeedExit
            
            // Multiply by speed vector
            mulps xmm0, [g_SpeedVector]

        SpeedExit:
            jmp [SpeedPlayerReturn]
        }
    }

    struct SpeedPlayerPatch : AutoAssemblerCodeHolder_Base
    {
        SpeedPlayerPatch(uintptr_t addr) {
            PresetScript_InjectJump(addr, (uintptr_t)&SpeedPlayerHook, 7);
        }
    };

    // =========================================================
    // 7. Mission Timer Hook
    // =========================================================
    DEFINE_HOOK(MissionTimerHook, MissionTimerReturn)
    {
        __asm {
            mov [g_pTimer], ecx // Capture pointer

            // Original Code:
            mov edx, [ecx+0x48]
            mov eax, [ecx+0x4C]

            jmp [MissionTimerReturn]
        }
    }

    struct MissionTimerPatch : AutoAssemblerCodeHolder_Base
    {
        MissionTimerPatch(uintptr_t addr) {
            PresetScript_InjectJump(addr, (uintptr_t)&MissionTimerHook, 6);
        }
    };

    // =========================================================
    // 8. DayTimeMgr Hook (Time Speed) - CE-style fTimeDelay approach
    // =========================================================
    // CE captures pDayTimeMgr and loads from fTimeDelay instead of [esi+A0].
    // CE: Lower fTimeDelay = faster time, Higher = slower, 48.0 = normal, 0 = stuck at 24.
    //
    // We use g_TimeScale which defaults to 48.0 (normal speed).
    // Slider writes to g_TimeScale, hook reads from g_TimeScale instead of memory.
    static void* captured_pDayTimeMgr = nullptr;
    
    DEFINE_HOOK(DayTimeMgrHook, DayTimeMgrReturn)
    {
        __asm {
            // Capture pDayTimeMgr from ESI (like CE: mov [pDayTimeMgr],esi)
            mov [captured_pDayTimeMgr], esi
            
            // Load from our global g_TimeScale instead of [esi+0xA0]
            // This is like CE's: movss xmm1, [fTimeDelay]
            movss xmm1, [g_TimeScale]
            
            jmp [DayTimeMgrReturn]
        }
    }

    struct DayTimeMgrPatch : AutoAssemblerCodeHolder_Base
    {
        DayTimeMgrPatch(uintptr_t addr) {
            // Hook the 8-byte movss xmm1,[esi+A0] instruction
            PresetScript_InjectJump(addr, (uintptr_t)&DayTimeMgrHook, 8);
        }
    };

    // =========================================================
    // 9. Map Manager Hook (Waypoint)
    // =========================================================
    // CE Hooks `AssassinsCreedIIGame.exe+EAA94B` : 8B 49 18 (mov ecx, [ecx+18])
    DEFINE_HOOK(MapManagerHook, MapManagerReturn)
    {
        __asm {
            mov ecx, [ecx+0x18]
            mov [captured_pMapManager], ecx // Capture pointer
            test ecx, ecx
            jmp [MapManagerReturn]
        }
    }
    struct MapManagerPatch : AutoAssemblerCodeHolder_Base {
        MapManagerPatch(uintptr_t addr) {
            PresetScript_InjectJump(addr, (uintptr_t)&MapManagerHook, 5); // 8B 49 18 85 C9
        }
    };

    // =========================================================
    // 10. Notoriety Hook (NEW - matches CE 120_Player Status.asm)
    // =========================================================
    // CE AOB: F3 0F 10 41 0C F3 0F 11 45 FC
    // CE captures ECX as pNotoriety, notoriety value is at [pNotoriety+0x0C]
    // This is a GETTER function - captures NotorietyManager pointer
    DEFINE_HOOK(NotorietyHook, NotorietyReturn)
    {
        __asm {
            // Capture pNotoriety (ECX contains NotorietyManager pointer)
            mov [captured_pNotoriety], ecx
            
            // Original code: movss xmm0, [ecx+0x0C]
            movss xmm0, [ecx+0x0C]
            
            // Check if we should disable notoriety
            cmp [bDisableNotoriety], 1
            jne NotorietyExit
            
            // Force xmm0 to 0.0 (notoriety = 0)
            xorps xmm0, xmm0
            
        NotorietyExit:
            jmp [NotorietyReturn]
        }
    }

    struct NotorietyPatch : AutoAssemblerCodeHolder_Base {
        NotorietyPatch(uintptr_t addr) {
            PresetScript_InjectJump(addr, (uintptr_t)&NotorietyHook, 5); // F3 0F 10 41 0C (5 bytes)
        }
    };

    // =========================================================
    // Wrappers & Initialization
    // =========================================================
    using HealthWrapper = AutoAssembleWrapper<HealthPatch>;
    using FallDamageWrapper = AutoAssembleWrapper<FallDamagePatch>;
    using LockConsumablesWrapper = AutoAssembleWrapper<LockConsumablesPatch>;
    using LockKnivesWrapper = AutoAssembleWrapper<LockKnivesPatch>;
    using TeleportWrapper = AutoAssembleWrapper<TeleportPatch>;
    using SpeedPlayerWrapper = AutoAssembleWrapper<SpeedPlayerPatch>;
    using MissionTimerWrapper = AutoAssembleWrapper<MissionTimerPatch>;
    using DayTimeMgrWrapper = AutoAssembleWrapper<DayTimeMgrPatch>;
    using MapManagerWrapper = AutoAssembleWrapper<MapManagerPatch>;
    using NotorietyWrapper = AutoAssembleWrapper<NotorietyPatch>;

    std::unique_ptr<HealthWrapper> s_Health;
    std::unique_ptr<FallDamageWrapper> s_FallDamage;
    std::unique_ptr<LockConsumablesWrapper> s_LockConsumables;
    std::unique_ptr<LockKnivesWrapper> s_LockKnives;
    std::unique_ptr<TeleportWrapper> s_Teleport;
    std::unique_ptr<SpeedPlayerWrapper> s_SpeedPlayer;
    std::unique_ptr<MissionTimerWrapper> s_MissionTimer;
    std::unique_ptr<DayTimeMgrWrapper> s_DayTimeMgr;
    std::unique_ptr<MapManagerWrapper> s_MapManager;
    std::unique_ptr<NotorietyWrapper> s_Notoriety;


    // =========================================================
    // Hook Installation Macro
    // =========================================================
    // Reduces boilerplate for installing hooks. Each hook needs:
    // - name: Hook name (e.g., Health -> uses HealthReturn, s_Health, HealthWrapper)
    // - aob: Pattern to scan for
    // - offset: Bytes to skip for return address
    #define INSTALL_HOOK(name, aob, offset) \
        do { \
            auto scan = Utils::PatternScanner::ScanMain(aob); \
            if (scan.found) { \
                name##Return = scan.address + (offset); \
                s_##name = std::make_unique<name##Wrapper>(scan.address); \
                s_##name->Activate(); \
                LOG_INFO("[AC2 Trainer] " #name " Hook installed."); \
            } else { \
                LOG_ERROR("[AC2 Trainer] " #name " AOB not found!"); \
            } \
        } while(0)

    void Initialize()
    {
        // Player Cheats Hooks
        INSTALL_HOOK(Health,          "8B 48 58 8B 55 08",                              0x06);
        INSTALL_HOOK(FallDamage,      "81 E6 FF 3F 00 00 8B C1",                        0x06);
        INSTALL_HOOK(SpeedPlayer,     "0F 28 86 A0 00 00 00 0F 29 85 50 FF FF FF 0F",   0x07);
        INSTALL_HOOK(Notoriety,       "F3 0F 10 41 0C F3 0F 11 45 FC",                  0x05);

        // Inventory Hooks
        INSTALL_HOOK(LockConsumables, "2B C2 89 41 10 B0 01 5D C2 04",                  0x05);
        INSTALL_HOOK(LockKnives,      "C1 F8 1F 23 D0",                                 0x05);

        // Teleport Hooks
        INSTALL_HOOK(Teleport,        "51 0F 29 40 30",                                 0x05);
        INSTALL_HOOK(MapManager,      "8B 49 18 85 C9 74 09",                           0x05);

        // World/Time Hooks
        INSTALL_HOOK(MissionTimer,    "8B 51 48 8B 41 4C 56",                           0x06);
        INSTALL_HOOK(DayTimeMgr,      "F3 0F 10 8E A0 00 00 00 F3 0F 5A",               0x08);
    }

    #undef INSTALL_HOOK

    void Shutdown()
    {
        s_Health.reset();
        s_FallDamage.reset();
        s_LockConsumables.reset();
        s_LockKnives.reset();
        s_Teleport.reset();
        s_SpeedPlayer.reset();
        s_MissionTimer.reset();
        s_DayTimeMgr.reset();
        s_MapManager.reset();
        s_Notoriety.reset();
    }

    void Update()
    {
        // Update Globals for Hooks
        g_pPlayer = AC2::GetPlayer();
        
        auto* bhv = AC2::GetBhvAssassin();
        if (bhv) g_pBiped = bhv->m_pBiped;
        
        // Map Config to Static Bools for ASM hooks
        bInfiniteItems = g_config.InfiniteItems;
        bFreeRoam = g_config.FlyMode; 
        bIgnoreFallDamage = g_config.IgnoreFallDamage;
        bGodMode = g_config.GodMode;
        bDisableNotoriety = g_config.DisableNotoriety;

        // Update Speed Vector
        float s = g_config.MovementSpeed;
        g_SpeedVector[0] = s;
        g_SpeedVector[1] = s;
        g_SpeedVector[2] = s;
        g_SpeedVector[3] = s;
    }

    // Called from TrainerPlugin::SyncHooks
    void SyncPointers()
    {
        AC2::g_pMissionTimer = g_pTimer;
        AC2::g_pPlayerHealth = captured_pHealth;
        AC2::g_pMapManager = captured_pMapManager;
    }

    // Get the captured NotorietyManager pointer (notoriety value is at +0x0C)
    void* GetNotorietyPointer()
    {
        return captured_pNotoriety;
    }

    // Get the captured DayTimeMgr pointer (time scale is at +0xA0)
    void* GetDayTimeMgrPointer()
    {
        return captured_pDayTimeMgr;
    }

    // Time scale control for the slider
    void SetTimeScale(float scale)
    {
        g_TimeScale = scale;
    }

    float GetTimeScale()
    {
        return g_TimeScale;
    }
}