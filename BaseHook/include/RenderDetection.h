#pragma once
#include <kiero/kiero.h>

namespace BaseHook
{
    namespace Util
    {
        // Detects the render type based on the executable name of the current process.
        kiero::RenderType::Enum GetGameSpecificRenderType();
    }
}