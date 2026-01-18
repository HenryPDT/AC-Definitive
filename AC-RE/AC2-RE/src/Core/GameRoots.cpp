#include "Core/GameRoots.h"
#include "PatternScanner.h"
#include "log.h"
#include <string_view>

namespace AC2
{
    GameRoots Roots = { 0 };

    namespace Patterns
    {
        using namespace std::string_view_literals;

        // Patterns sourced 1-to-1 from Paul44's 9.2 Cheat Table
        constexpr auto BhvAssChain     = "8A 41 2C 84 C0"sv;
        constexpr auto WhiteRoom       = "57 33 FF 85 F6 74 75"sv;
        constexpr auto TimeOfDay       = "08 F2 0F 59 C2 0F 5A C9"sv;
        constexpr auto ProgressionMgr  = "8B 80 80 02 00 00 50"sv;
        constexpr auto CharacterSave   = "89 75 E8 83 C6 04 57"sv;
        constexpr auto MapManager      = "8B 49 18 85 C9 74 09"sv;
        constexpr auto SpeedSystem     = "8B 30 8B 48 04 89 7D"sv;
        constexpr auto Notoriety       = "F3 0F 10 41 0C F3 0F 11 45 FC"sv;
    }

    void InitializeRoots()
    {
        LOG_INFO("[AC2] Initializing Game Roots via AOB Scan...");

        using AutoAssemblerKinda::PatternScanner;

        // pBhvAssChain: offset -0x06
        if (auto scan = PatternScanner::ScanMain(Patterns::BhvAssChain))
            Roots.BhvAssassinChain = scan.ExtractAbsoluteAddress(-0x06).m_Address;
        
        // pWhiteRoom / TimeOfDayManager: offset -0x06
        if (auto scan = PatternScanner::ScanMain(Patterns::WhiteRoom))
            Roots.TimeOfDayManager = scan.ExtractAbsoluteAddress(-0x06).m_Address;

        // pTimeOfDay (Current Global Time): instruction at scan + 0x0C
        if (auto scan = PatternScanner::ScanMain(Patterns::TimeOfDay))
            Roots.CurrentTimeGlobal = scan.ExtractAbsoluteAddress(0x0C).m_Address;

        // pProgressionMgr: offset -0x06 from pattern match
        if (auto scan = PatternScanner::ScanMain(Patterns::ProgressionMgr))
            Roots.ProgressionManager = scan.ExtractAbsoluteAddress(-0x06).m_Address;

        // pSwitchCharSave: offset -0x06
        if (auto scan = PatternScanner::ScanMain(Patterns::CharacterSave))
            Roots.CharacterSave = scan.ExtractAbsoluteAddress(-0x06).m_Address;

        // pSpeedSystem: offset -0x05
        if (auto scan = PatternScanner::ScanMain(Patterns::SpeedSystem))
            Roots.SpeedSystem = scan.ExtractAbsoluteAddress(-0x05).m_Address;

        // Camera: Found 8 bytes after BhvAssassinChain root
        if (Roots.BhvAssassinChain) {
            Roots.Camera = Roots.BhvAssassinChain + 0x08;
        }

        LOG_INFO("BhvChain: %p, ToD: %p, Prog: %p", 
            (void*)Roots.BhvAssassinChain, (void*)Roots.TimeOfDayManager, (void*)Roots.ProgressionManager);

        LOG_INFO("[AC2] Roots initialization complete.");
    }
}