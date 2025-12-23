#pragma once

#include <Windows.h>
#include <memory>

#include "PluginManager.h"
#include "ImGuiConsole.h"
#include "IPlugin.h"

class PluginLoaderApp
{
public:
    explicit PluginLoaderApp(HMODULE module);
    ~PluginLoaderApp();

    PluginLoaderApp(const PluginLoaderApp&) = delete;
    PluginLoaderApp& operator=(const PluginLoaderApp&) = delete;

    void Init();
    void Tick(); // called from the loader thread (non-render thread)
    void RequestShutdown();
    bool IsShutdownRequested() const;

    void Shutdown();

    PluginManager& GetPluginManager() { return m_pluginManager; }
    ImGuiConsole& GetConsole() { return m_console; }
    PluginLoaderInterface& GetLoaderInterface() { return m_loaderInterface; }
    HMODULE GetModule() const { return m_module; }

    static PluginLoaderApp* Get();

private:
    struct LoaderSettings;

    static PluginLoaderApp* s_instance;

    HMODULE m_module = nullptr;
    bool m_shutdownRequested = false;

    PluginManager m_pluginManager;
    ImGuiConsole m_console;
    PluginLoaderInterface m_loaderInterface;
    std::unique_ptr<LoaderSettings> m_settings;
};


