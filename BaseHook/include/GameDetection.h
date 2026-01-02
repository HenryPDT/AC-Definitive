#pragma once
#include "IPlugin.h"

namespace BaseHook
{
    namespace Util
    {
        // Detects the game based on the executable name.
        Game GetCurrentGame();
    }
}