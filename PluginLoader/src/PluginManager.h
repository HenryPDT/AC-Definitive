#pragma once
#include <Windows.h>
#include <vector>
#include <memory>
#include <string>
#include "IPlugin.h"

struct LoadedPlugin {
    HMODULE handle;
    std::unique_ptr<IPlugin> instance;
    std::string name;
};

class PluginManager
{
public:
    void Init(HMODULE loaderModule, PluginLoaderInterface& loaderInterface);
    void ShutdownPlugins();
    void UpdatePlugins();
    void RenderPluginMenus();
    void DrawPluginMenu();
    Game GetCurrentGame() const { return m_currentGame; }

private:
    void DetectGame();
    void LoadPlugins(PluginLoaderInterface& loaderInterface);

    std::vector<LoadedPlugin> m_plugins;
    Game m_currentGame = Game::Unknown;
    HMODULE m_loaderModule = NULL;
};