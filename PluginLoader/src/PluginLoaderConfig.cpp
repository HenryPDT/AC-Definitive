#include "PluginLoaderConfig.h"
#include "Serialization/Serialization.h"
#include "Serialization/Utils/FileSystem.h"
#include "log.h"

namespace PluginLoaderConfig {

    Config g_Config;
    fs::path g_ConfigFilepath;
    fs::file_time_type g_LastWriteTime;

    void Init(HMODULE hModule)
    {
        char modulePath[MAX_PATH];
        GetModuleFileNameA(hModule, modulePath, MAX_PATH);
        g_ConfigFilepath = std::filesystem::path(modulePath).replace_extension(".json");
    }

    void Load()
    {
        if (g_ConfigFilepath.empty()) return;
        if (fs::exists(g_ConfigFilepath))
        {
            // Update timestamp first to avoid loop if load fails or writes back
            g_LastWriteTime = fs::last_write_time(g_ConfigFilepath);

            Serialization::JSON cfg = Serialization::Utils::LoadJSONFromFile(g_ConfigFilepath);
            g_Config.SectionFromJSON(cfg);
            LOG_INFO("Config loaded.");
        }
        else
        {
            Save();
        }
    }

    void Save()
    {
        if (g_ConfigFilepath.empty()) return;
        Serialization::JSON cfg;
        g_Config.SectionToJSON(cfg);
        Serialization::Utils::SaveJSONToFile(cfg, g_ConfigFilepath);
        
        if (fs::exists(g_ConfigFilepath))
             g_LastWriteTime = fs::last_write_time(g_ConfigFilepath);
    }

    void CheckHotReload()
    {
        if (g_ConfigFilepath.empty()) return;
        
        // Simple polling (could be optimized, but file IO stat is cheap enough for 100ms sleep loop)
        std::error_code ec;
        if (fs::exists(g_ConfigFilepath, ec))
        {
            auto currentWriteTime = fs::last_write_time(g_ConfigFilepath, ec);
            if (!ec && currentWriteTime > g_LastWriteTime)
            {
                LOG_INFO("Config change detected on disk. Reloading...");
                Load();
            }
        }
    }
}