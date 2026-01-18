#pragma once
#include <IPlugin.h>
#include <Serialization/Config.h>
#include <KeyBind.h>

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
        PROPERTY(PlayerSpeed, float, Serialization::NumericAdapter_template<float>, 1.0f)  // BipedComponent animation speed
        PROPERTY(GlobalSpeed, float, Serialization::NumericAdapter_template<float>, 1.0f)  // SpeedSystem global speed
        PROPERTY(PlayerScale, float, Serialization::NumericAdapter_template<float>, 1.0f)
        
        // Misc
        PROPERTY(SkipCredits, bool, Serialization::BooleanAdapter, false)

        // World / Time
        PROPERTY(TimeOfDay, float, Serialization::NumericAdapter_template<float>, 12.0f)
        PROPERTY(PauseTime, bool, Serialization::BooleanAdapter, false)
        PROPERTY(FreezeMissionTimer, bool, Serialization::BooleanAdapter, false)

        // Teleport / Free Roam
        PROPERTY(FreeRoamTarget, int, Serialization::NumericAdapter_template<int>, 0)  // 0=Off, 1=Player, 2=Camera
        PROPERTY(FlySpeed, float, Serialization::NumericAdapter_template<float>, 1.0f)
        PROPERTY(CameraFOV, float, Serialization::NumericAdapter_template<float>, 0.81f)
        PROPERTY(LockPlayerInCameraMode, bool, Serialization::BooleanAdapter, true)

        // Keybinds
        PROPERTY(Key_TeleportWaypoint, KeyBind, Serialization::KeyBindAdapter, KeyBind('T', false, true, false)) // Shift+T
        PROPERTY(Key_SavePosition, KeyBind, Serialization::KeyBindAdapter, KeyBind(VK_F11))
        PROPERTY(Key_RestorePosition, KeyBind, Serialization::KeyBindAdapter, KeyBind(VK_F12))
        PROPERTY(Key_SkipBink, KeyBind, Serialization::KeyBindAdapter, KeyBind('V', true, false, false)) // Ctrl+V
        PROPERTY(Key_FlyForward, KeyBind, Serialization::KeyBindAdapter, KeyBind('W'))
        PROPERTY(Key_FlyBackward, KeyBind, Serialization::KeyBindAdapter, KeyBind('S'))
        PROPERTY(Key_FlyLeft, KeyBind, Serialization::KeyBindAdapter, KeyBind('A'))
        PROPERTY(Key_FlyRight, KeyBind, Serialization::KeyBindAdapter, KeyBind('D'))
        PROPERTY(Key_FlyUp, KeyBind, Serialization::KeyBindAdapter, KeyBind(VK_SPACE))
        PROPERTY(Key_FlyDown, KeyBind, Serialization::KeyBindAdapter, KeyBind(VK_CONTROL))
    };
}
