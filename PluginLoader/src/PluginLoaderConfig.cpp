#include "PluginLoaderConfig.h"
#include "Serialization/Serialization.h"
#include "Serialization/Utils/FileSystem.h"

namespace PluginLoaderConfig {

    Config g_Config;
    fs::path g_ConfigFilepath;

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
            Serialization::JSON cfg = Serialization::Utils::LoadJSONFromFile(g_ConfigFilepath);
            g_Config.SectionFromJSON(cfg);
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
    }
}
