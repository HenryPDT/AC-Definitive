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

        using Utils::PatternScanner;

        // pBhvAssChain: offset -0x06
        auto scanBhv = PatternScanner::ScanMain(Patterns::BhvAssChain);
        if (scanBhv) Roots.BhvAssassinChain = scanBhv.ExtractAbsoluteAddress(-0x06).address;
        else LOG_ERROR("Failed to find BhvAssassinChain pattern");
        
        // pWhiteRoom / TimeOfDayManager: offset -0x06
        auto scanWhite = PatternScanner::ScanMain(Patterns::WhiteRoom);
        if (scanWhite) Roots.TimeOfDayManager = scanWhite.ExtractAbsoluteAddress(-0x06).address;
        else LOG_ERROR("Failed to find TimeOfDayManager pattern");

        // pTimeOfDay (Current Global Time): instruction at scan + 0x0C
        auto scanTime = PatternScanner::ScanMain(Patterns::TimeOfDay);
        if (scanTime) Roots.CurrentTimeGlobal = scanTime.ExtractAbsoluteAddress(0x0C).address;
        else LOG_ERROR("Failed to find CurrentTimeGlobal pattern");

        // pProgressionMgr: offset -0x06 from pattern match
        auto scanProg = PatternScanner::ScanMain(Patterns::ProgressionMgr);
        if (scanProg) Roots.ProgressionManager = scanProg.ExtractAbsoluteAddress(-0x06).address;
        else LOG_ERROR("Failed to find ProgressionManager pattern");

        // pSwitchCharSave: offset -0x06
        auto scanChar = PatternScanner::ScanMain(Patterns::CharacterSave);
        if (scanChar) Roots.CharacterSave = scanChar.ExtractAbsoluteAddress(-0x06).address;
        else LOG_ERROR("Failed to find CharacterSave pattern");

        // pSpeedSystem: offset -0x05
        auto scanSpeed = PatternScanner::ScanMain(Patterns::SpeedSystem);
        if (scanSpeed) Roots.SpeedSystem = scanSpeed.ExtractAbsoluteAddress(-0x05).address;
        else LOG_ERROR("Failed to find SpeedSystem pattern");

        // Camera: Found 8 bytes after BhvAssassinChain root
        if (Roots.BhvAssassinChain) {
            Roots.Camera = Roots.BhvAssassinChain + 0x08;
        }

        LOG_INFO("BhvChain: %p, ToD: %p, Prog: %p", 
            (void*)Roots.BhvAssassinChain, (void*)Roots.TimeOfDayManager, (void*)Roots.ProgressionManager);

        LOG_INFO("[AC2] Roots initialization complete.");
    }
}