#pragma once

#include <filesystem>

#include <Serialization/Serialization.h>
#include <Serialization/Utils/FileSystem.h>

#include "PluginUtils.h"

namespace PluginConfig
{
    // Generic helper for plugins:
    // - Reads config from:
    //    1) <module_dir>/config/<module_stem>.json if it exists
    //    2) else legacy <module>.json next to the plugin
    // - Always writes to <module_dir>/config/<module_stem>.json (migration + single config root)
    template <class ConfigT>
    inline std::filesystem::path Load(ConfigT& config, const void* pluginEntryAddress)
    {
        const auto hMod = PluginUtils::ModuleFromAddress(pluginEntryAddress);
        const auto modPath = PluginUtils::ModulePath(hMod);
        const auto rootDir = PluginUtils::ConfigRootDir(pluginEntryAddress);

        std::error_code ec;
        std::filesystem::create_directories(rootDir, ec); // best-effort

        const auto configPath = rootDir / (modPath.stem().string() + ".json");

        Serialization::JSON jsonConfig = Serialization::Utils::LoadJSONFromFile(configPath);
        if (!jsonConfig.IsNull())
            config.SectionFromJSON(jsonConfig);

        Serialization::JSON outJson;
        config.SectionToJSON(outJson);
        Serialization::Utils::SaveJSONToFile(outJson, configPath);

        return configPath;
    }
}


