#pragma once
#include <cstdint>

namespace AC2::Constants
{
    // God Mode flags (SharedData+0x20)
    constexpr uint8_t GOD_MODE_ON = 0x81;
    constexpr uint8_t GOD_MODE_OFF = 0x80;

    // Time of Day defaults
    constexpr float TIME_DELAY_DEFAULT = 48.0f;  // Normal day/night cycle speed

    // Teleport defaults
    constexpr float DROP_HEIGHT_DEFAULT = 40.0f;
    constexpr float WAYPOINT_Z_OFFSET = 5.0f;

    // Inventory IDs
    constexpr uint32_t KNIFE_ITEM_ID = 0xAFD4F6F3;
}
