#pragma once
#include <Windows.h>
#include <vector>
#include <memory>
#include <string>
#include "IPlugin.h"

struct LoadedPlugin {
    HMODULE handle = NULL;
    std::unique_ptr<IPlugin> instance;
    std::string name;

    LoadedPlugin(HMODULE h, std::unique_ptr<IPlugin> i, std::string n)
        : handle(h), instance(std::move(i)), name(std::move(n)) {}

    // RAII Destructor to ensure DLL is freed only after plugin is destroyed
    ~LoadedPlugin() {
        instance.reset(); // Explicitly destroy plugin instance first
        if (handle) {
            FreeLibrary(handle);
        }
    }

    // Move constructor only
    LoadedPlugin(LoadedPlugin&& other) noexcept {
        handle = other.handle;
        instance = std::move(other.instance);
        name = std::move(other.name);
        other.handle = NULL; // Steal ownership so other doesn't FreeLibrary
    }

    LoadedPlugin& operator=(LoadedPlugin&&) = delete;
    LoadedPlugin(const LoadedPlugin&) = delete;
    LoadedPlugin& operator=(const LoadedPlugin&) = delete;
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