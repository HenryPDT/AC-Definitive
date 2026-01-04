#pragma once
#include <IPlugin.h>
#include <Serialization/Config.h>

extern const PluginLoaderInterface* g_loader_ref;

namespace Trainer
{
    struct Configuration : Serialization::ConfigSection
    {
        SECTION_CTOR(Configuration)

        // Player Status Cheats
        PROPERTY(GodMode, bool, Serialization::BooleanAdapter, false)
        PROPERTY(Invisible, bool, Serialization::BooleanAdapter, false)
        PROPERTY(InfiniteItems, bool, Serialization::BooleanAdapter, false)
        PROPERTY(DisableNotoriety, bool, Serialization::BooleanAdapter, false)
        PROPERTY(IgnoreFallDamage, bool, Serialization::BooleanAdapter, false)
        
        // Player Stats
        PROPERTY(MovementSpeed, float, Serialization::NumericAdapter_template<float>, 1.0f)
        PROPERTY(PlayerScale, float, Serialization::NumericAdapter_template<float>, 1.0f)
        
        // World / Time
        PROPERTY(TimeOfDay, float, Serialization::NumericAdapter_template<float>, 12.0f)
        PROPERTY(PauseTime, bool, Serialization::BooleanAdapter, false)
        PROPERTY(FreezeMissionTimer, bool, Serialization::BooleanAdapter, false)

        // Teleport / Free Roam
        PROPERTY(FlyMode, bool, Serialization::BooleanAdapter, false)
        PROPERTY(FlySpeed, float, Serialization::NumericAdapter_template<float>, 10.0f)
    };
}
