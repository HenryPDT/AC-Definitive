#include "ui/MainMenu.h"

#include "ImGuiCTX.h"
#include "base.h"

#include "imgui.h"

namespace Ui
{
    void MainMenu::Draw(PluginManager& pluginManager)
    {
        if (!m_initialized)
        {
            m_settings.LoadFromConfig();
            m_initialized = true;
        }

        m_settings.SyncVirtualResolutionIfNeeded();
        m_settings.SyncDetectedDisplayStateIfNeeded();

        if (ImGuiCTX::Window window("AC Definitive", &BaseHook::Data::bShowMenu); window)
        {
            if (ImGuiCTX::TabBar tabBar("MainTabBar"); tabBar)
            {
                if (ImGuiCTX::Tab tabPlugins("Plugins"); tabPlugins)
                {
                    const char* gameNames[] = { "Unknown", "AC1", "AC2", "ACB", "ACR" };
                    int gameIdx = (int)pluginManager.GetCurrentGame();
                    if (gameIdx < 0 || gameIdx > 4) gameIdx = 0;
                    ImGui::Text("Detected Game: %s", gameNames[gameIdx]);
                    ImGui::Separator();

                    pluginManager.DrawPluginMenu();
                }

                if (ImGuiCTX::Tab tabSettings("Settings"); tabSettings)
                {
                    if (ImGui::CollapsingHeader("Window Management", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        m_settings.DrawWindowedModeSection();
                    }

                    if (ImGui::CollapsingHeader("Framerate"))
                    {
                        m_settings.DrawFramerateSection();
                    }

                    if (ImGui::CollapsingHeader("CPU Affinity"))
                    {
                        m_settings.DrawCpuAffinitySection();
                    }

                    if (ImGui::CollapsingHeader("Hotkeys"))
                    {
                        m_settings.DrawHotkeysSection();
                    }

                    if (ImGui::CollapsingHeader("Appearance"))
                    {
                        m_settings.DrawAppearanceSection();
                    }

                    if (ImGui::CollapsingHeader("Input"))
                    {
                        m_settings.DrawInputSection();
                    }

                    m_settings.DrawSaveRow();
                }

                if (ImGuiCTX::Tab tabAbout("About"); tabAbout)
                {
                    ImGui::Text("AC Definitive Framework");
                    ImGui::Separator();
                    ImGui::Text("Based on ACUFixes Plugin Loader.");
                    ImGui::Text("ImGui Version: %s", IMGUI_VERSION);
                }
            }
        }

        // Render individual plugin GUIs (they might create their own windows)
        pluginManager.RenderPluginMenus();
    }
}


