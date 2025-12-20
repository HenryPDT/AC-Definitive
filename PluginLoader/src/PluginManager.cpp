#include "PluginManager.h"
#include "log.h"
#include "imgui.h"
#include <filesystem>
#include <algorithm>

void PluginManager::Init(HMODULE loaderModule, PluginLoaderInterface& loaderInterface)
{
    m_loaderModule = loaderModule;
    DetectGame();
    LoadPlugins(loaderInterface);
}

void PluginManager::ShutdownPlugins()
{
    m_plugins.clear(); // Destructors will be called
}

void PluginManager::UpdatePlugins()
{
    for (auto& plugin : m_plugins)
    {
        plugin.instance->OnUpdate();
    }
}

void PluginManager::RenderPluginMenus()
{
    for (auto& plugin : m_plugins)
    {
        plugin.instance->OnGuiRender();
    }
}

void PluginManager::DrawPluginMenu()
{
    if (ImGui::CollapsingHeader("Loaded Plugins"))
    {
        for (const auto& plugin : m_plugins)
        {
            ImGui::BulletText("%s (v%d.%d)", plugin.name.c_str(), plugin.instance->GetPluginVersion() >> 16, plugin.instance->GetPluginVersion() & 0xFFFF);
        }
    }
}

void PluginManager::DetectGame()
{
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::filesystem::path gamePath(exePath);
    std::string exeName = gamePath.filename().string();

    if (_stricmp(exeName.c_str(), "AssassinsCreed_Dx9.exe") == 0) m_currentGame = Game::AC1;
    else if (_stricmp(exeName.c_str(), "AssassinsCreed_Dx10.exe") == 0) m_currentGame = Game::AC1;
    else if (_stricmp(exeName.c_str(), "AssassinsCreedIIGame.exe") == 0) m_currentGame = Game::AC2;
    else if (_stricmp(exeName.c_str(), "ACBSP.exe") == 0) m_currentGame = Game::ACB;
    else if (_stricmp(exeName.c_str(), "ACRSP.exe") == 0) m_currentGame = Game::ACR;

    LOG_INFO("Detected game: %d", (int)m_currentGame);
}

void* PluginManager::GetPluginInterface(const std::string& name)
{
    for (const auto& plugin : m_plugins)
    {
        if (plugin.name == name)
        {
            return plugin.instance->GetInterface();
        }
    }
    return nullptr;
}

void PluginManager::LoadPlugins(PluginLoaderInterface& loaderInterface)
{
    char loaderPath[MAX_PATH];
    GetModuleFileNameA(m_loaderModule, loaderPath, MAX_PATH);
    std::filesystem::path pluginDir = std::filesystem::path(loaderPath).parent_path() / "plugins";

    if (!std::filesystem::exists(pluginDir))
    {
        LOG_INFO("Plugins directory does not exist, no plugins will be loaded.");
        return;
    }

    std::vector<std::filesystem::path> pluginFiles;
    for (const auto& entry : std::filesystem::directory_iterator(pluginDir))
    {
        if (entry.path().extension() == ".asi")
        {
            pluginFiles.push_back(entry.path());
        }
    }

    std::sort(pluginFiles.begin(), pluginFiles.end());

    for (const auto& path : pluginFiles)
    {
        LOG_INFO("Attempting to load plugin: %s", path.string().c_str());
        HMODULE hPlugin = LoadLibraryW(path.wstring().c_str());
        if (!hPlugin)
        {
            LOG_ERROR("Could not load plugin: %s. Error: %lu", path.string().c_str(), GetLastError());
            continue;
        }

        auto pluginEntry = (PluginEntrypoint)GetProcAddress(hPlugin, "PluginEntry");
        if (!pluginEntry)
        {
            LOG_ERROR("Could not find PluginEntry export in %s", path.string().c_str());
            FreeLibrary(hPlugin);
            continue;
        }

        IPlugin* plugin_instance = pluginEntry();
        if (!plugin_instance)
        {
            LOG_ERROR("PluginEntry for %s returned nullptr.", path.string().c_str());
            FreeLibrary(hPlugin);
            continue;
        }

        uint32_t pluginVersion = plugin_instance->GetPluginAPIVersion();
        uint32_t loaderVersion = g_PluginLoaderAPIVersion;

        // Check for Major version mismatch (ABI break) or if plugin is newer than loader
        if ((pluginVersion >> 16) != (loaderVersion >> 16) || pluginVersion > loaderVersion)
        {
            LOG_ERROR("Plugin %s is incompatible. Plugin Version: %d.%d, Loader Version: %d.%d",
                path.string().c_str(),
                pluginVersion >> 16, pluginVersion & 0xFFFF,
                loaderVersion >> 16, loaderVersion & 0xFFFF);
            delete plugin_instance;
            FreeLibrary(hPlugin);
            continue;
        }

        LOG_INFO("Loaded plugin: %s", plugin_instance->GetPluginName());
        m_plugins.emplace_back(hPlugin, std::unique_ptr<IPlugin>(plugin_instance), std::string(plugin_instance->GetPluginName()));
        plugin_instance->OnPluginInit(loaderInterface);
    }
}
