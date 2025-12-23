#pragma once

#include "PluginManager.h"
#include "ui/SettingsModel.h"

namespace Ui
{
    class MainMenu
    {
    public:
        void Draw(PluginManager& pluginManager);

    private:
        SettingsModel m_settings;
        bool m_initialized = false;
    };
}


