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
    // =========================================================
    // Globals accessible by naked functions
    // =========================================================
    static AC2::Entity* g_pPlayer = nullptr;
    static AC2::BipedComponent* g_pBiped = nullptr;
    static AC2::MissionTimer* g_pTimer = nullptr;
    
    // Pointers captured by hooks
    AC2::CSrvPlayerHealth* captured_pHealth = nullptr;
    AC2::MapManager* captured_pMapManager = nullptr;
    static void* captured_pNotoriety = nullptr;
    static void* captured_pFreeRoam = nullptr;
    static void* captured_pMapManage = nullptr;
    static void* captured_pFreeCamera = nullptr;
    static void* captured_pFreeCam = nullptr;
    static void* captured_pDayTimeMgr = nullptr;
    
    // Camera fly mode state
    static int nFreeRoamTarget = 0;
    __declspec(align(16)) float g_CameraPos[4] = { 0, 0, 0, 0 };
    
    // Hook state flags
    static bool bInfiniteItems = false;
    static bool bFreeRoam = false;
    static bool bIgnoreFallDamage = false;
    static bool bGodMode = false;
    static bool bDisableNotoriety = false;
    static float g_TimeScale = AC2::Constants::TIME_DELAY_DEFAULT;
    __declspec(align(16)) float g_SpeedVector[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

    // =========================================================
    // 1. Health Hook
    // =========================================================
    DEFINE_AOB_HOOK(HealthHook, 
        "00 CC CC CC CC CC CC CC CC CC 55 8B EC 8B 41 0C 8B 48 58 8B 55 08", 
        0x10, 6
    );

    HOOK_IMPL(HealthHook) {
        __asm {
            mov [captured_pHealth], eax
            mov ecx, [eax+0x58]
            mov edx, [ebp+0x08]
            jmp [HealthHook_Return]
        }
    }

    // =========================================================
    // 2. Fall Damage Hook
    // =========================================================
    DEFINE_AOB_HOOK(FallDamageHook, 
        "16 81 E6 FF 3F 00 00 8B C1", 
        0x01, 6
    );

    HOOK_IMPL(FallDamageHook) {
        __asm {
            and esi, 0x3FFF
            push eax
            mov eax, [g_pPlayer]
            test eax, eax
            je FallDamageExit
            cmp ebx, eax
            jne FallDamageExit
            cmp [bIgnoreFallDamage], 1
            jne FallDamageExit
            mov dword ptr [ebp+0x0C], 0
        FallDamageExit:
            pop eax
            jmp [FallDamageHook_Return]
        }
    }

    // =========================================================
    // 3. Lock Consumables Hook
    // =========================================================
    DEFINE_AOB_HOOK(LockConsumablesHook, 
        "2B C2 89 41 10 B0 01 5D C2 04", 
        0, 5
    );

    HOOK_IMPL(LockConsumablesHook) {
        __asm {
            cmp [bInfiniteItems], 1
            jne LockConsumablesOriginal
            mov [ecx+0x10], eax
            jmp LockConsumablesExit
        LockConsumablesOriginal:
            sub eax, edx
            mov [ecx+0x10], eax
        LockConsumablesExit:
            jmp [LockConsumablesHook_Return]
        }
    }

    // =========================================================
    // 4. Lock Knives Hook
    // =========================================================
    DEFINE_AOB_HOOK(LockKnivesHook, 
        "00 CC CC 55 8B EC 56 8B 75 0C 8B", 
        0x11, 5
    );

    HOOK_IMPL(LockKnivesHook) {
        __asm {
            sar eax, 0x1F
            and edx, eax
            jmp [LockKnivesHook_Return]
        }
    }

    // =========================================================
    // 5. Teleport Hook
    // =========================================================
    DEFINE_AOB_HOOK(TeleportHook, 
        "51 0F 29 40 30", 
        0, 5
    );

    HOOK_IMPL(TeleportHook) {
        __asm {
            push ebx
            mov ebx, eax
            sub ebx, 0x10
            cmp ebx, [g_pPlayer]
            pop ebx
            jne TeleportOriginal
            cmp [bFreeRoam], 1
            jne TeleportOriginal
            movaps xmm0, [eax+0x30]
        TeleportOriginal:
            push ecx
            movaps [eax+0x30], xmm0
            jmp [TeleportHook_Return]
        }
    }

    // =========================================================
    // 6. Speed Player Hook
    // =========================================================
    DEFINE_AOB_HOOK(SpeedPlayerHook, 
        "0F 28 86 A0 00 00 00 0F 29 85 50 FF FF FF 0F", 
        0, 7
    );

    HOOK_IMPL(SpeedPlayerHook) {
        __asm {
            push ebx
            mov ebx, [esi+0x08]
            cmp ebx, [g_pPlayer]
            jne SpeedOriginal
            mov bl, byte ptr [esi+0x1A]
            cmp bl, 0
            je SpeedOriginal
            movaps xmm0, [esi+0xA0]
            mulps xmm0, [g_SpeedVector]
            movaps [esi+0xA0], xmm0
        SpeedOriginal:
            pop ebx
            movaps xmm0, [esi+0xA0]
            jmp [SpeedPlayerHook_Return]
        }
    }

    // =========================================================
    // 7. Mission Timer Hook
    // =========================================================
    DEFINE_AOB_HOOK(MissionTimerHook, 
        "8B 51 48 8B 41 4C 56", 
        0, 6
    );

    HOOK_IMPL(MissionTimerHook) {
        __asm {
            mov [g_pTimer], ecx
            mov edx, [ecx+0x48]
            mov eax, [ecx+0x4C]
            jmp [MissionTimerHook_Return]
        }
    }

    // =========================================================
    // 8. DayTimeMgr Hook
    // =========================================================
    DEFINE_AOB_HOOK(DayTimeMgrHook, 
        "F3 0F 10 8E A0 00 00 00 F3 0F 5A", 
        0, 8
    );

    HOOK_IMPL(DayTimeMgrHook) {
        __asm {
            mov [captured_pDayTimeMgr], esi
            movss xmm1, [g_TimeScale]
            jmp [DayTimeMgrHook_Return]
        }
    }

    // =========================================================
    // 9. Map Manager Hook
    // =========================================================
    DEFINE_AOB_HOOK(MapManagerHook, 
        "8B 49 18 85 C9 74 09 8B 03", 
        0, 5
    );

    HOOK_IMPL(MapManagerHook) {
        __asm {
            mov ecx, [ecx+0x18]
            mov [captured_pMapManager], ecx
            test ecx, ecx
            jmp [MapManagerHook_Return]
        }
    }

    // =========================================================
    // 10. Notoriety Hook
    // =========================================================
    DEFINE_AOB_HOOK(NotorietyHook, 
        "F3 0F 10 41 0C F3 0F 11 45 FC", 
        0, 5
    );

    HOOK_IMPL(NotorietyHook) {
        __asm {
            mov [captured_pNotoriety], ecx
            movss xmm0, [ecx+0x0C]
            cmp [bDisableNotoriety], 1
            jne NotorietyExit
            xorps xmm0, xmm0
        NotorietyExit:
            jmp [NotorietyHook_Return]
        }
    }

    // =========================================================
    // 11. FreeCam Hook
    // =========================================================
    DEFINE_AOB_HOOK(FreeCamHook, 
        "8B 17 F3 0F 10 46 0C", 
        0, 7
    );

    HOOK_IMPL(FreeCamHook) {
        __asm {
            mov [captured_pFreeRoam], edi
            mov [captured_pFreeCam], esi
            mov edx, [edi]
            movss xmm0, [esi+0x0C]
            jmp [FreeCamHook_Return]
        }
    }

    // =========================================================
    // 12. MapManage Hook
    // =========================================================
    DEFINE_AOB_HOOK(MapManageHook, 
        "0F B6 48 24 D9 5D F0", 
        0, 7
    );

    HOOK_IMPL(MapManageHook) {
        __asm {
            mov [captured_pMapManage], eax
            movzx ecx, byte ptr [eax+0x24]
            fstp dword ptr [ebp-0x10]
            jmp [MapManageHook_Return]
        }
    }

    // =========================================================
    // 13. FreeCamera Hook
    // =========================================================
    DEFINE_AOB_HOOK(FreeCameraHook, 
        "0F 28 40 30 0F 29 41 30 E8", 
        0, 8
    );

    HOOK_IMPL(FreeCameraHook) {
        __asm {
            mov [captured_pFreeCamera], ecx
            movaps xmm0, [eax+0x30]
            cmp [nFreeRoamTarget], 2
            jne FreeCameraOriginal
            push edx
            mov edx, [g_CameraPos]
            mov [ecx+0x30], edx
            mov edx, [g_CameraPos+4]
            mov [ecx+0x34], edx
            mov edx, [g_CameraPos+8]
            mov [ecx+0x38], edx
            pop edx
            movaps xmm0, [ecx+0x30]
            jmp FreeCameraExit
        FreeCameraOriginal:
            movaps [ecx+0x30], xmm0
        FreeCameraExit:
            jmp [FreeCameraHook_Return]
        }
    }

    // =========================================================
    // Initialize
    // =========================================================
    void Initialize()
    {
        // Use unified HookManager to resolve and install all hooks
        // Using requireUnique=true to ensure safety and detect ambiguous patterns
        size_t installed = HookManager::ResolveAndInstallAll(true);
        LOG_INFO("[Hooks] Initialized. Installed %zu hooks.", installed);
    }

    void Shutdown()
    {
        HookManager::UninstallAll();
    }

    void Update()
    {
        g_pPlayer = AC2::GetPlayer();
        
        auto* bhv = AC2::GetBhvAssassin();
        if (bhv) g_pBiped = bhv->m_pBiped;
        
        bInfiniteItems = g_config.InfiniteItems;
        // Freeze player position in Player fly mode, OR in Camera mode with lock enabled
        bFreeRoam = (g_config.FreeRoamTarget == 1) || 
                    (g_config.FreeRoamTarget == 2 && g_config.LockPlayerInCameraMode);
        bIgnoreFallDamage = g_config.IgnoreFallDamage;
        bGodMode = g_config.GodMode;
        bDisableNotoriety = g_config.DisableNotoriety;
        nFreeRoamTarget = g_config.FreeRoamTarget;

        float s = g_config.PlayerSpeed;
        g_SpeedVector[0] = s;
        g_SpeedVector[1] = s;
        g_SpeedVector[2] = s;
        g_SpeedVector[3] = s;
    }

    void SyncPointers()
    {
        AC2::g_pMissionTimer = g_pTimer;
        AC2::g_pPlayerHealth = captured_pHealth;
        AC2::g_pMapManager = captured_pMapManager;
    }

    void* GetNotorietyPointer() { return captured_pNotoriety; }
    void* GetDayTimeMgrPointer() { return captured_pDayTimeMgr; }
    void SetTimeScale(float scale) { g_TimeScale = scale; }
    float GetTimeScale() { return g_TimeScale; }
    void* GetFreeRoamPointer() { return captured_pFreeRoam; }
    void* GetMapManagePointer() { return captured_pMapManage; }
    void* GetFreeCameraObjectPointer() { return captured_pFreeCamera; }
    float* GetCameraPosPointer() { return g_CameraPos; }
    void* GetFreeCamPointer() { return captured_pFreeCam; }
}