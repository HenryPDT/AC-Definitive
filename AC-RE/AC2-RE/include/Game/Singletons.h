#pragma once

namespace AC2
{
    class BhvAssassin;
    class Entity;
    class World;
    class Inventory;
    class TimeOfDayManager;
    class ProgressionManager;
    class MapManager;
    class SpeedSystem;
    class CSrvPlayerHealth;
    class CharacterAI;
    class MissionTimer;
    class SharedData;
    class Bink;

    BhvAssassin* GetBhvAssassin();
    Entity* GetPlayer();
    CSrvPlayerHealth* GetPlayerHealth();
    SharedData* GetSharedData();
    
    // Globals set by hooks
    // These are captured dynamically when the game runs specific code paths
    extern CSrvPlayerHealth* g_pPlayerHealth;
    extern Bink* g_pBink;
    extern MapManager* g_pMapManager;
    extern MissionTimer* g_pMissionTimer;

    World* GetWorld();
    Inventory* GetInventory();
    TimeOfDayManager* GetTimeOfDayManager();
    bool IsInWhiteRoom();
    float* GetCurrentTimeGlobal();
    ProgressionManager* GetProgressionManager();
    MapManager* GetMapManager();
    SpeedSystem* GetSpeedSystem();
    MissionTimer* GetMissionTimer();
    Bink* GetBink();
    void** GetCharacterSave();
}

