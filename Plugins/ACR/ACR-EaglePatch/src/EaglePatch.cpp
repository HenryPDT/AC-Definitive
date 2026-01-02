#include <windows.h>
#include "IPlugin.h"
#include "imgui.h"
#include "EaglePatch.h"
#include "Controller.h"
#include "SkipIntro.h"
#include <PluginConfig.h>
#include <CpuAffinity.h>
#include <filesystem>

// Define the global loader reference here
const PluginLoaderInterface* g_loader_ref = nullptr;
ACREaglePatch::Configuration g_config;
std::filesystem::path g_configPath;
static bool g_imgui_context_set = false;

class ACREaglePatchPlugin : public IPlugin
{
public:
    const char* GetPluginName() override { return "ACR EaglePatch"; }
    uint32_t GetPluginVersion() override { return MAKE_PLUGIN_API_VERSION(1, 0); }

    void OnPluginInit(const PluginLoaderInterface& loader_interface) override
    {
        g_loader_ref = &loader_interface;
        g_loader_ref->LogToFile("[ACR EaglePatch] Initializing...");

        uintptr_t baseAddr = (uintptr_t)GetModuleHandleA(NULL);
        auto version = ACR::DetectVersion(baseAddr);

        // --- Load Configuration ---
        g_configPath = PluginConfig::Load(g_config, (const void*)PluginEntry);

        if (version != ACREaglePatch::GameVersion::Unknown)
        {
            if (g_config.EnableXInput)
                ACREaglePatch::InitController(baseAddr, version, g_config.KeyboardLayout);
            if (g_config.SkipIntroVideos)
                ACREaglePatch::InitSkipIntro(baseAddr, version);

            if (g_config.FixCpuAffinity)
            {
                uint64_t mask = CpuAffinity::GetSystemAffinityMask() & ~1ULL;
                CpuAffinity::Apply(mask);
            }
        }
        else
            g_loader_ref->LogToConsole("[ACR EaglePatch] Unknown Game Version!");
    }

    void OnGuiRender() override
    {
        // Sync ImGui context if not set
        if (!g_imgui_context_set && g_loader_ref && g_loader_ref->m_ImGuiContext)
        {
            ImGui::SetCurrentContext(g_loader_ref->m_ImGuiContext);
            g_imgui_context_set = true;
        }
        if (!g_imgui_context_set) return;

        if (ImGui::CollapsingHeader("Requires Restart", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Checkbox("Hybrid Input (Simultaneous KB/M + Controller)", &g_config.EnableXInput.get());
            ImGui::Checkbox("Skip Intro Videos", &g_config.SkipIntroVideos.get());
            if (ImGui::Checkbox("Fix CPU Affinity (Disable Core 0)", &g_config.FixCpuAffinity.get()))
            {
                if (g_config.FixCpuAffinity)
                    CpuAffinity::Apply(CpuAffinity::GetSystemAffinityMask() & ~1ULL);
                else
                    CpuAffinity::Apply(CpuAffinity::GetSystemAffinityMask());
            }

            const char* layouts[] = { "Keyboard / Mouse (2 buttons)", "Keyboard / Mouse (5 buttons)", "Keyboard / Mouse (Alt)", "Keyboard", "Keyboard (Alt)" };
            int currentLayout = g_config.KeyboardLayout;
            if (currentLayout < 0 || currentLayout > 4) currentLayout = 0;
            if (ImGui::Combo("Keyboard Layout", &currentLayout, layouts, IM_ARRAYSIZE(layouts)))
            {
                g_config.KeyboardLayout = currentLayout;
                ACREaglePatch::UpdateKeyboardLayout(currentLayout);
            }
        }

        ImGui::Separator();
        if (ImGui::Button("Save Settings"))
        {
            Serialization::JSON outJson;
            g_config.SectionToJSON(outJson);
            if (Serialization::Utils::SaveJSONToFile(outJson, g_configPath) && g_loader_ref)
                g_loader_ref->LogToConsole("[ACR EaglePatch] Config saved.");
        }
        ImGui::TextDisabled("Note: 'Requires Restart' settings do not apply instantly.");
    }
};

extern "C" __declspec(dllexport) IPlugin* PluginEntry()
{
    return new ACREaglePatchPlugin();
}

