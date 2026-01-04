#include "PluginManager.h"
#include "log.h"
#include "imgui.h"
#include <filesystem>
#include <algorithm>
#include "imgui_internal.h"
#include "util/GameDetection.h"

void PluginManager::Init(HMODULE loaderModule, PluginLoaderInterface& loaderInterface)
{
    m_loaderModule = loaderModule;
    m_currentGame = BaseHook::Util::GetCurrentGame();
    LOG_INFO("Detected game: %d", (int)m_currentGame);
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
        if (plugin.showWindow)
        {
            // Set default size/pos for new windows
            ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(500, 500), ImGuiCond_FirstUseEver);

            // Render in a separate window
            if (ImGui::Begin(plugin.name.c_str(), &plugin.showWindow, ImGuiWindowFlags_NoFocusOnAppearing))
            {
                plugin.instance->OnGuiRender();
            }
            ImGui::End();
        }
    }
}

void PluginManager::DrawPluginMenu()
{
    // Use a child region for the list to keep it tidy if list is long
    if (ImGui::BeginChild("PluginList", ImVec2(0, 0), false, ImGuiWindowFlags_None))
    {
        for (auto& plugin : m_plugins)
        {
            ImGui::PushID(plugin.name.c_str());

            const bool poppedOut = plugin.showWindow;
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Framed |
                                       ImGuiTreeNodeFlags_AllowOverlap |
                                       ImGuiTreeNodeFlags_FramePadding |
                                       ImGuiTreeNodeFlags_SpanAvailWidth;

            if (poppedOut)
            {
                // If popped out, treat as leaf to prevent inline expansion
                flags |= ImGuiTreeNodeFlags_Leaf;
                // Optional: distinct color for popped out items
                ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetStyle().Colors[ImGuiCol_Header]);
            }

            // Draw Header
            bool isNodeOpen = ImGui::TreeNodeEx(plugin.name.c_str(), flags, "%s (v%d.%d)",
                plugin.name.c_str(),
                plugin.instance->GetPluginVersion() >> 16,
                plugin.instance->GetPluginVersion() & 0xFFFF);

            if (poppedOut)
            {
                ImGui::PopStyleColor();
                // Feature: Clicking the header of a popped-out plugin focuses its window
                if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
                {
                    ImGui::SetWindowFocus(plugin.name.c_str());
                }
            }

            // Draw Toggle Button aligned to right
            const char* btnLabel = poppedOut ? "Attach" : "Detach";
            ImGuiStyle& style = ImGui::GetStyle();
            
            float btnWidth = ImGui::CalcTextSize(btnLabel).x + style.FramePadding.x * 2.0f;
            float availWidth = ImGui::GetContentRegionAvail().x;
            float btnX = ImGui::GetCursorPosX() + availWidth - btnWidth - style.ItemSpacing.x;

            // Ensure button doesn't overlap text if window is very narrow
            if (btnX < ImGui::GetCursorPosX() + 100.0f) btnX = ImGui::GetCursorPosX() + 100.0f;

            ImGui::SameLine();
            ImGui::SetCursorPosX(btnX);
            
            // Use a distinct color for the "Attach" state to make it obvious
            if (poppedOut) ImGui::PushStyleColor(ImGuiCol_Button, style.Colors[ImGuiCol_ButtonActive]);

            if (ImGui::SmallButton(btnLabel))
            {
                plugin.showWindow = !plugin.showWindow;
            }

            if (poppedOut) ImGui::PopStyleColor();

            // Tooltip
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip(poppedOut ? "Attach this plugin back into the list" : "Detach this plugin into a separate window");
            }

            if (isNodeOpen)
            {
                if (!poppedOut)
                {
                    // Inline render
                    // Auto-resize height and border for the embedded child
                    if (ImGui::BeginChild("embedded", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY))
                    {
                        ImGui::Dummy(ImVec2(0, 2.0f));
                        plugin.instance->OnGuiRender();
                        ImGui::Dummy(ImVec2(0, 2.0f));
                    }
                    ImGui::EndChild();
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
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
