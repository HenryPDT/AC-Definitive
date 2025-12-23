#include "SettingsModel.h"

#include "CpuAffinity.h"
#include "ImGuiConfigUtils.h"
#include "log.h"

#include "imgui.h"

// Windowed Mode helpers removed for rebase

void SettingsModel::LoadFromConfig()
{
    // Windowed Mode config loading removed for rebase
    
    m_draft.cpuAffinityMask = PluginLoaderConfig::g_Config.CpuAffinityMask.get();
    m_systemAffinityMask = CpuAffinity::GetSystemAffinityMask();

    m_draft.toggleMenu = PluginLoaderConfig::g_Config.hotkey_ToggleMenu.get();
    m_draft.toggleConsole = PluginLoaderConfig::g_Config.hotkey_ToggleConsole.get();
    m_draft.fontSize = PluginLoaderConfig::g_Config.fontSize.get();
}

void SettingsModel::SyncVirtualResolutionIfNeeded()
{
    // Implementation removed for rebase
}

/*
void SettingsModel::ApplyWindowedModeToRuntime(bool allowFakeReset)
{
    // Implementation removed for rebase
}
*/

void SettingsModel::ApplyCpuAffinityToRuntime()
{
    if (m_draft.cpuAffinityMask == 0)
        m_draft.cpuAffinityMask = (m_systemAffinityMask ? m_systemAffinityMask : 1ULL);

    PluginLoaderConfig::g_Config.CpuAffinityMask = m_draft.cpuAffinityMask;
    CpuAffinity::Apply(m_draft.cpuAffinityMask);
}

/*
void SettingsModel::DrawWindowedModeSection()
{
    // Implementation removed for rebase
}
*/

void SettingsModel::DrawCpuAffinitySection()
{
    ImGui::Separator();
    ImGui::Text("CPU Affinity");

    if (m_systemAffinityMask == 0)
        m_systemAffinityMask = CpuAffinity::GetSystemAffinityMask();

    // --- Presets Row 1 ---
    if (ImGui::Button("Default (All)"))
    {
        m_draft.cpuAffinityMask = m_systemAffinityMask;
        ApplyCpuAffinityToRuntime();
    }
    ImGui::SameLine();
    if (ImGui::Button("Disable SMT"))
    {
        m_draft.cpuAffinityMask = m_systemAffinityMask & 0x5555555555555555ULL;
        ApplyCpuAffinityToRuntime();
    }

    // --- Presets Row 2 ---
    if (ImGui::Button("AC1 Fix (<32 Cores)"))
    {
        m_draft.cpuAffinityMask = 0x7FFFFFFFULL & m_systemAffinityMask;
        ApplyCpuAffinityToRuntime();
    }
    ImGui::SameLine();
    if (ImGui::Button("AC2/B/R Fix (No Core 0)"))
    {
        m_draft.cpuAffinityMask = m_systemAffinityMask & ~1ULL;
        ApplyCpuAffinityToRuntime();
    }

    ImGui::Dummy(ImVec2(0, 5));
    ImGui::Text("Manual Core Selection:");

    if (ImGui::BeginChild("CpuList", ImVec2(0, 150), true, ImGuiWindowFlags_None))
    {
        ImGui::Columns(2, "CpuCols", false);
        for (int i = 0; i < 64; ++i)
        {
            if (!((m_systemAffinityMask >> i) & 1ULL))
                break;

            char label[32];
            snprintf(label, sizeof(label), "CPU %d", i);

            bool enabled = ((m_draft.cpuAffinityMask >> i) & 1ULL) != 0;
            if (ImGui::Checkbox(label, &enabled))
            {
                if (enabled) m_draft.cpuAffinityMask |= (1ULL << i);
                else m_draft.cpuAffinityMask &= ~(1ULL << i);

                if (m_draft.cpuAffinityMask == 0)
                    m_draft.cpuAffinityMask = (1ULL << i);

                ApplyCpuAffinityToRuntime();
            }
            ImGui::NextColumn();
        }
        ImGui::Columns(1);
    }
    ImGui::EndChild();
}

void SettingsModel::DrawHotkeysSection()
{
    ImGui::Separator();
    ImGui::Text("Hotkeys");

    if (ImGui::KeyBindInput("Toggle Menu", m_draft.toggleMenu))
        PluginLoaderConfig::g_Config.hotkey_ToggleMenu = m_draft.toggleMenu;
    if (ImGui::KeyBindInput("Toggle Console", m_draft.toggleConsole))
        PluginLoaderConfig::g_Config.hotkey_ToggleConsole = m_draft.toggleConsole;
}

void SettingsModel::DrawAppearanceSection()
{
    ImGui::Separator();
    ImGui::Text("Appearance");

    if (ImGui::DragInt("Font Size", &m_draft.fontSize, 1, 8, 32))
        PluginLoaderConfig::g_Config.fontSize = m_draft.fontSize;
}

bool SettingsModel::DrawSaveRow()
{
    ImGui::Separator();

    bool saved = false;
    if (ImGui::Button("Save Config"))
    {
        PluginLoaderConfig::Save();
        saved = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Save Window Layout (imgui.ini)"))
        ImGui::SaveIniSettingsToDisk(ImGui::GetIO().IniFilename);

    static bool show_demo = false;
    if (ImGui::Button("Show ImGui Demo Window"))
        show_demo = !show_demo;
    if (show_demo)
        ImGui::ShowDemoWindow(&show_demo);

    return saved;
}