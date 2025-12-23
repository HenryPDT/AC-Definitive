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

        if (ImGuiCTX::Window window("AC Definitive", &BaseHook::Data::bShowMenu); window)
        {
            if (ImGuiCTX::TabBar tabBar("MainTabBar"); tabBar)
            {
                if (ImGuiCTX::Tab tabPlugins("Plugins"); tabPlugins)
                {
                    pluginManager.DrawPluginMenu();
                    ImGui::Separator();

                    const char* gameNames[] = { "Unknown", "AC1", "AC2", "ACB", "ACR" };
                    int gameIdx = (int)pluginManager.GetCurrentGame();
                    if (gameIdx < 0 || gameIdx > 4) gameIdx = 0;
                    ImGui::Text("Detected Game: %s", gameNames[gameIdx]);
                }

                if (ImGuiCTX::Tab tabSettings("Settings"); tabSettings)
                {
                    ImGui::Checkbox("Fix DirectInput (Legacy Input)", &BaseHook::Data::bFixDirectInput);

                    // m_settings.DrawWindowedModeSection(); // Removed for rebase
                    m_settings.DrawCpuAffinitySection();
                    m_settings.DrawHotkeysSection();
                    m_settings.DrawAppearanceSection();
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