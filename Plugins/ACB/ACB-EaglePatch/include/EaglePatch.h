#pragma once
#include <IPlugin.h>
#include <cstdint>
#include <windows.h>
#include <Serialization/Config.h>
#include <GameVersion.h>

// Global reference to the loader interface
extern const PluginLoaderInterface* g_loader_ref;

namespace ACBEaglePatch
{
    using GameVersion = ACB::GameVersion;

    struct Configuration : Serialization::ConfigSection
    {
        SECTION_CTOR(Configuration)

        PROPERTY(EnableXInput, bool, Serialization::BooleanAdapter, true)
        PROPERTY(SkipIntroVideos, bool, Serialization::BooleanAdapter, true)
        PROPERTY(KeyboardLayout, int, Serialization::IntegerAdapter_template<int>, 0)
        PROPERTY(FixCpuAffinity, bool, Serialization::BooleanAdapter, true)
    };
}