#include "pch.h"
#include "GameDetection.h"
#include "RenderDetection.h"
#include <filesystem>
#include <algorithm>
#include <string>

namespace BaseHook
{
    namespace Util
    {
        static bool EqualsIgnoreCase(const std::string& a, const char* b)
        {
            return _stricmp(a.c_str(), b) == 0;
        }

        kiero::RenderType::Enum GetGameSpecificRenderType()
        {
            char modulePath[MAX_PATH];
            if (GetModuleFileNameA(NULL, modulePath, MAX_PATH) == 0) return kiero::RenderType::None;

            std::filesystem::path path(modulePath);
            std::string exeName = path.filename().string();
            
            // Assassin's Creed 1
            if (EqualsIgnoreCase(exeName, "AssassinsCreed_Dx9.exe")) return kiero::RenderType::D3D9;
            if (EqualsIgnoreCase(exeName, "AssassinsCreed_Dx10.exe")) return kiero::RenderType::D3D10;
            
            // Assassin's Creed 2
            if (EqualsIgnoreCase(exeName, "AssassinsCreedIIGame.exe")) return kiero::RenderType::D3D9;
            
            // Assassin's Creed Brotherhood
            if (EqualsIgnoreCase(exeName, "ACBSP.exe")) return kiero::RenderType::D3D9;
            
            // Assassin's Creed Revelations
            if (EqualsIgnoreCase(exeName, "ACRSP.exe")) return kiero::RenderType::D3D9;
            
            // Assassin's Creed 3 (Original)
            if (EqualsIgnoreCase(exeName, "AC3SP.exe")) return kiero::RenderType::D3D11;
            
            // Assassin's Creed 4: Black Flag
            if (EqualsIgnoreCase(exeName, "AC4BFSP.exe")) return kiero::RenderType::D3D11;
            
            return kiero::RenderType::None;
        }

        Game GetCurrentGame()
        {
            char modulePath[MAX_PATH];
            if (GetModuleFileNameA(NULL, modulePath, MAX_PATH) == 0) return Game::Unknown;

            std::filesystem::path path(modulePath);
            std::string exeName = path.filename().string();

            if (EqualsIgnoreCase(exeName, "AssassinsCreed_Dx9.exe") || EqualsIgnoreCase(exeName, "AssassinsCreed_Dx10.exe")) return Game::AC1;
            if (EqualsIgnoreCase(exeName, "AssassinsCreedIIGame.exe")) return Game::AC2;
            if (EqualsIgnoreCase(exeName, "ACBSP.exe")) return Game::ACB;
            if (EqualsIgnoreCase(exeName, "ACRSP.exe")) return Game::ACR;
            if (EqualsIgnoreCase(exeName, "AC3SP.exe")) return Game::AC3;
            if (EqualsIgnoreCase(exeName, "AC4BFSP.exe")) return Game::AC4;

            return Game::Unknown;
        }
    }
}
