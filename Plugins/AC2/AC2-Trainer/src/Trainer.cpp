#include <windows.h>
#include "Trainer.h"
#include "IPlugin.h"
#include "imgui.h"
#include <ImGuiConfigUtils.h>
#include "Core/GameRoots.h"
#include "Hooks.h"
#include "Cheats/PlayerCheats.h"
#include "Cheats/InventoryCheats.h"
#include "Cheats/WorldCheats.h"
#include "Cheats/TeleportCheats.h"
#include "Cheats/CharacterCheats.h"
#include <PluginConfig.h>
#include <filesystem>
#include <memory>
#include <vector>
#include "log.h"

// Global references
const PluginLoaderInterface* g_loader_ref = nullptr;
Trainer::Configuration g_config;
std::filesystem::path g_configPath;
static bool g_imgui_context_set = false;

class TrainerPlugin : public IPlugin
{
private:

    std::unique_ptr<PlayerCheats> m_PlayerCheats;
    std::unique_ptr<InventoryCheats> m_InventoryCheats;
    std::unique_ptr<WorldCheats> m_WorldCheats;
    std::unique_ptr<TeleportCheats> m_TeleportCheats;
    std::unique_ptr<CharacterCheats> m_CharacterCheats;

public:
    const char* GetPluginName() override { return "AC2 Trainer"; }
    uint32_t GetPluginVersion() override { return MAKE_PLUGIN_API_VERSION(1, 0); }

    ~TrainerPlugin() {
        Hooks::Shutdown();
    }

    void OnPluginInit(const PluginLoaderInterface& loader_interface) override
    {
        g_loader_ref = &loader_interface;

        // Configure logging to forward to loader
        Log::InitSink(g_loader_ref->LogToConsole);

        LOG_INFO("[AC2 Trainer] Initializing...");
        
        AC2::InitializeRoots();
        Hooks::Initialize();

        // Load Config
        g_configPath = PluginConfig::Load(g_config, (const void*)PluginEntry);

        // Initialize Cheat Modules
        m_PlayerCheats = std::make_unique<PlayerCheats>();
        m_InventoryCheats = std::make_unique<InventoryCheats>();
        m_WorldCheats = std::make_unique<WorldCheats>();
        m_TeleportCheats = std::make_unique<TeleportCheats>();
        m_CharacterCheats = std::make_unique<CharacterCheats>();

        // Apply initial config states if needed
    }

    void OnGuiRender() override
    {
        if (!g_imgui_context_set && g_loader_ref && g_loader_ref->m_ImGuiContext)
        {
            ImGui::SetCurrentContext(g_loader_ref->m_ImGuiContext);
            g_imgui_context_set = true;
        }
        if (!g_imgui_context_set) return;

        // Draw the UI directly here or delegate to modules
        if (ImGui::BeginTabBar("TrainerTabs"))
        {
            // Tab 1: Player - Health, cheats, scale, speed
            if (ImGui::BeginTabItem("Player"))
            {
                m_PlayerCheats->DrawUI();
                ImGui::Spacing();
                m_PlayerCheats->DrawMiscUI();
                ImGui::EndTabItem();
            }

            // Tab 2: Inventory - Money and items
            if (ImGui::BeginTabItem("Inventory"))
            {
                m_InventoryCheats->DrawUI();
                ImGui::EndTabItem();
            }

            // Tab 3: World - Time, mission timer, map
            if (ImGui::BeginTabItem("World"))
            {
                m_WorldCheats->DrawUI();
                ImGui::EndTabItem();
            }

            // Tab 4: Camera & Movement - Teleport, free roam, FOV
            if (ImGui::BeginTabItem("Camera & Movement"))
            {
                m_TeleportCheats->DrawUI();
                ImGui::EndTabItem();
            }

            // Tab 5: Character - Character switcher
            if (ImGui::BeginTabItem("Character"))
            {
                m_CharacterCheats->DrawUI();
                ImGui::EndTabItem();
            }

            // Tab 6: Settings - Keybinds
            if (ImGui::BeginTabItem("Settings"))
            {
                // Keybinds
                ImGui::TextDisabled("-- Keybinds --");
                
                ImGui::KeyBindInput("Teleport to Waypoint", g_config.Key_TeleportWaypoint.get());
                ImGui::KeyBindInput("Save Position", g_config.Key_SavePosition.get());
                ImGui::KeyBindInput("Restore Position", g_config.Key_RestorePosition.get());
                
                ImGui::Separator();
                ImGui::TextDisabled("-- Fly Mode Controls --");
                
                ImGui::KeyBindInput("Fly Forward", g_config.Key_FlyForward.get());
                ImGui::KeyBindInput("Fly Backward", g_config.Key_FlyBackward.get());
                ImGui::KeyBindInput("Fly Left", g_config.Key_FlyLeft.get());
                ImGui::KeyBindInput("Fly Right", g_config.Key_FlyRight.get());
                ImGui::KeyBindInput("Fly Up", g_config.Key_FlyUp.get());
                ImGui::KeyBindInput("Fly Down", g_config.Key_FlyDown.get());

                ImGui::Separator();
                
                if (ImGui::Button("Save Configuration"))
                {
                    Serialization::JSON outJson;
                    g_config.SectionToJSON(outJson);
                    if (Serialization::Utils::SaveJSONToFile(outJson, g_configPath))
                        LOG_INFO("[AC2 Trainer] Config saved.");
                }
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }

    void OnUpdate() override
    {
        Hooks::Update();
        Hooks::SyncPointers();
        if (m_PlayerCheats) m_PlayerCheats->Update();
        if (m_InventoryCheats) m_InventoryCheats->Update();
        if (m_WorldCheats) m_WorldCheats->Update();
        if (m_TeleportCheats) m_TeleportCheats->Update();
    }
};

extern "C" __declspec(dllexport) IPlugin* PluginEntry()
{
    return new TrainerPlugin();
}