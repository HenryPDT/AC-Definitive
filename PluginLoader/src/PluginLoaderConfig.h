#pragma once

#include <windows.h>
#include "Serialization/Config.h"
#include "Serialization/Adapters/NumericAdapters.h"
#include "Serialization/Adapters/EnumAdapter.h"
#include "Serialization/Adapters/HexAdapter.h"
#include "KeyBind.h"
#include <filesystem>
namespace fs = std::filesystem;

namespace PluginLoaderConfig {

    struct Config : Serialization::ConfigSection
    {
        SECTION_CTOR(Config);

        PROPERTY(hotkey_ToggleMenu, KeyBind, Serialization::KeyBindAdapter, KeyBind(VK_INSERT));
        PROPERTY(hotkey_ToggleConsole, KeyBind, Serialization::KeyBindAdapter, KeyBind(VK_OEM_3)); // Tilde
        PROPERTY(fontSize, float, Serialization::NumericAdapter_template<float>, 13.0f);

        // CPU Settings
        PROPERTY(CpuAffinityMask, uint64_t, Serialization::HexStringAdapter, 0);
    };

    extern Config g_Config;
    extern fs::path g_ConfigFilepath;

    void Init(HMODULE hModule);
    void Load();
    void Save();
    void CheckHotReload();
}
