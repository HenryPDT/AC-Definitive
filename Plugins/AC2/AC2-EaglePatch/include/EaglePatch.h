#pragma once
#include <IPlugin.h>
#include <cstdint>
#include <windows.h>
#include <Serialization/Config.h>
#include <GameVersion.h>

// Global reference to the loader interface
extern const PluginLoaderInterface* g_loader_ref;

namespace AC2EaglePatch
{
    using GameVersion = AC2::GameVersion;

    struct Configuration : Serialization::ConfigSection
    {
        SECTION_CTOR(Configuration)

        // Original EaglePatchAC2.ini mapping
        PROPERTY(ImproveDrawDistance, bool, Serialization::BooleanAdapter, true)
        PROPERTY(ImproveShadowMapResolution, bool, Serialization::BooleanAdapter, true)
        PROPERTY(KeyboardLayout, int, Serialization::IntegerAdapter_template<int>, 0)
        PROPERTY(SkipIntroVideos, bool, Serialization::BooleanAdapter, true)
        PROPERTY(UnlockFPS, bool, Serialization::BooleanAdapter, true)
        PROPERTY(UPlayItems, bool, Serialization::BooleanAdapter, true)
        PROPERTY(EnableXInput, bool, Serialization::BooleanAdapter, true)
        PROPERTY(FixCpuAffinity, bool, Serialization::BooleanAdapter, true)
    };
}