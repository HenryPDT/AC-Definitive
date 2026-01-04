#pragma once
#include <cstdint>

namespace AC2
{
    /**
     * @brief GameRoots holds the resolved absolute addresses of global variables
     * found via AOB scanning. These serve as the entry points for all game logic.
     */
    struct GameRoots
    {
        uintptr_t BhvAssassinChain;     // The root for player and world data
        uintptr_t TimeOfDayManager;     // The root for time and loading state
        uintptr_t CurrentTimeGlobal;    // Global float for the current hour
        uintptr_t ProgressionManager;   // Root for character and profile data
        uintptr_t CharacterSave;        // Pointer to character save state
        uintptr_t MapManager;           // Root for map and waypoints
        uintptr_t SpeedSystem;          // Root for the global speed multiplier
        uintptr_t Camera;               // Root for camera properties
    };

    extern GameRoots Roots;

    /**
     * @brief Scans the process memory for the patterns defined in the Cheat Table
     * and populates the Roots structure.
     */
    void InitializeRoots();
}