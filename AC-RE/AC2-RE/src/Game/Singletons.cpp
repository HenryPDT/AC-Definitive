#include "Game/Singletons.h"
#include "Core/GameRoots.h"
#include "Game/BhvAssassin.h"
#include "Game/SharedData.h"
#include "Game/Managers/TimeOfDayManager.h"
#include "Game/Managers/ProgressionManager.h"
#include "Game/Managers/MapManager.h"
#include "Game/Managers/CSrvPlayerHealth.h"
#include "Game/Camera.h"
#include "Game/SpeedSystem.h"
#include "Game/Managers/MissionTimer.h"
#include "PatternScanner.h"
#include <windows.h>

namespace AC2
{
    using Utils::ScanResult;
    using Utils::PatternScanner;

    // Stored from hook
    MissionTimer* g_pMissionTimer = nullptr;
    MapManager* g_pMapManager = nullptr;
    CSrvPlayerHealth* g_pPlayerHealth = nullptr;

    BhvAssassin* GetBhvAssassin()
    {
        // CE Chain: [[[[pBhvAssChain]+0x20]+0x18]+0x0]
        // Note: Roots.BhvAssassinChain is the ADDRESS of the pointer variable.
        return PatternScanner::FromAddress(Roots.BhvAssassinChain)
            .Dereference()
            .ResolvePointerChain({ 0x20, 0x18, 0x0 }) // Offsets applied to base, then dereferenced
            .As<BhvAssassin*>();
    }

    Entity* GetPlayer()
    {
        auto* bhv = GetBhvAssassin();
        if (!bhv) return nullptr;
        // bhv->m_pEntity is a pointer. We validate the memory it points to.
        return PatternScanner::FromAddress((uintptr_t)bhv->m_pEntity).As<Entity*>();
    }

    SharedData* GetSharedData()
    {
        auto* bhv = GetBhvAssassin();
        if (!bhv || !bhv->m_pCharacterAI) return nullptr;
        
        auto* playerData = bhv->m_pCharacterAI->m_pPlayerData;
        if (!playerData) return nullptr;
        
        return playerData->m_pSharedData;
    }

    CSrvPlayerHealth* GetPlayerHealth()
    {
        // Prefer the hook pointer if available, as it's what CE uses
        if (g_pPlayerHealth) return g_pPlayerHealth;

        // Fallback to chain
        auto* player = GetPlayer();
        if (!player) return nullptr;

        // CE Chain: [[[[[Entity+74]+64]+10]+30]+40]
        return PatternScanner::FromAddress((uintptr_t)player->m_pHealthChainRoot)
            .ResolvePointerChain({ 0x64, 0x10, 0x30, 0x40 })
            .As<CSrvPlayerHealth*>();
    }

    World* GetWorld()
    {
        auto* bhv = GetBhvAssassin();
        if (!bhv) return nullptr;
        return PatternScanner::FromAddress((uintptr_t)bhv->m_pWorld).As<World*>();
    }

    Inventory* GetInventory()
    {
        auto* bhv = GetBhvAssassin();
        if (!bhv) return nullptr;

        // Check CharacterAI
        auto* charAI = PatternScanner::FromAddress((uintptr_t)bhv->m_pCharacterAI).As<CharacterAI*>();
        if (!charAI) return nullptr;

        // Check PlayerData
        auto* playerData = PatternScanner::FromAddress((uintptr_t)charAI->m_pPlayerData).As<PlayerDataItem*>();
        if (!playerData) return nullptr;

        // Check Inventory
        return PatternScanner::FromAddress((uintptr_t)playerData->m_pInventory).As<Inventory*>();
    }

    TimeOfDayManager* GetTimeOfDayManager()
    {
        // CE Chain: [[[[[pWhiteRoom]+0x8]+0x20]+0x10]+0x2C]
        return PatternScanner::FromAddress(Roots.TimeOfDayManager)
            .Dereference()
            .ResolvePointerChain({ 0x8, 0x20, 0x10, 0x2C })
            .As<TimeOfDayManager*>();
    }

    bool IsInWhiteRoom()
    {
        auto* mgr = GetTimeOfDayManager();
        if (!mgr) return true; // Default to true if not found/safe

        // Validate reading offset 0xD4 (size of uint32_t)
        // We can use a temporary ScanResult to check safety of offset
        auto check = PatternScanner::FromAddress((uintptr_t)mgr).Offset(0xD4).As<uint32_t*>();
        if (!check) return true;

        return *check != 0;
    }

    float* GetCurrentTimeGlobal()
    {
        // Roots.CurrentTimeGlobal is the address of the float variable
        return PatternScanner::FromAddress(Roots.CurrentTimeGlobal).As<float*>();
    }

    ProgressionManager* GetProgressionManager()
    {
        // CE pProgressionMgr points to the struct that has +70 as selected profile.
        // Roots.ProgressionManager (from -6 scan) is 1E134BC.
        // Dereferencing 1E134BC gives us the instance.
        // We do NOT add 0x280. The 0x280 offset in ASM is for a different object (EAX path).
        return PatternScanner::FromAddress(Roots.ProgressionManager)
            .Dereference()
            .As<ProgressionManager*>();
    }

    MapManager* GetMapManager()
    {
        if (g_pMapManager) return g_pMapManager;

        // Fallback to static root if hook hasn't fired
        return PatternScanner::FromAddress(Roots.MapManager).Dereference().As<MapManager*>();
    }

    void** GetCharacterSave()
    {
        // Roots.CharacterSave is the address of the pointer
        return PatternScanner::FromAddress(Roots.CharacterSave).As<void**>();
    }

    SpeedSystem* GetSpeedSystem()
    {
        return PatternScanner::FromAddress(Roots.SpeedSystem)
            .Dereference()
            .As<SpeedSystem*>();
    }

    MissionTimer* GetMissionTimer()
    {
        // Populated by Hooks::MissionTimerHook
        return g_pMissionTimer;
    }
}