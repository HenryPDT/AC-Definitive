#pragma once
#include <IPlugin.h>
#include <cstdint>
#include <windows.h>
#include <Serialization/Config.h>
#include <GameVersion.h>

// Global reference to the loader interface
extern const PluginLoaderInterface* g_loader_ref;

namespace AC1EaglePatch
{
    using GameVersion = AC1::GameVersion;

    struct Configuration : Serialization::ConfigSection
    {
        SECTION_CTOR(Configuration)

        // Original EaglePatchAC1.ini mapping
        PROPERTY(D3D10_RemoveDuplicateResolutions, bool, Serialization::BooleanAdapter, true)
        PROPERTY(KeyboardLayout, int, Serialization::IntegerAdapter_template<int>, 0)
        PROPERTY(SkipIntroVideos, bool, Serialization::BooleanAdapter, true)
        PROPERTY(EnableXInput, bool, Serialization::BooleanAdapter, true)

        // Features implied by EaglePatch but usually hardcoded
        PROPERTY(DisableTelemetry, bool, Serialization::BooleanAdapter, true)
        PROPERTY(MultisamplingFix, bool, Serialization::BooleanAdapter, true)
        PROPERTY(FixCpuAffinity, bool, Serialization::BooleanAdapter, true)
    };
}