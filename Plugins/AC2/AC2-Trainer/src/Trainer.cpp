#include <windows.h>
#include "Trainer.h"
#include "IPlugin.h"
#include "imgui.h"
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
            // Modlist: Player Status
            if (ImGui::BeginTabItem("Player Status"))
            {
                m_PlayerCheats->DrawUI();
                ImGui::EndTabItem();
            }

            // Modlist: Miscellaneous
            if (ImGui::BeginTabItem("Miscellaneous"))
            {
                // Time Of Day / Map
                m_WorldCheats->DrawUI();

                // Resize / Speed
                m_PlayerCheats->DrawMiscUI();

                // Switch Character
                ImGui::Separator();
                m_CharacterCheats->DrawUI();

                ImGui::EndTabItem();
            }

            // Modlist: Inventory
            if (ImGui::BeginTabItem("Inventory"))
            {
                m_InventoryCheats->DrawUI();
                ImGui::EndTabItem();
            }

            // Modlist: Teleport & Coordinates
            if (ImGui::BeginTabItem("Teleport & Coordinates"))
            {
                m_TeleportCheats->DrawUI();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Settings"))
            {
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