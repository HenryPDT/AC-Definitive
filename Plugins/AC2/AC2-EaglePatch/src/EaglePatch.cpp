#include <windows.h>
#include "IPlugin.h"
#include "imgui.h"
#include "EaglePatch.h"
#include "Controller.h"
#include "SkipIntro.h"
#include "Graphics.h"
#include "UplayBonus.h"
#include "FPSUnlock.h"
#include <PluginConfig.h>
#include <CpuAffinity.h>
#include <filesystem>
#include "log.h"

// Define the global loader reference here
const PluginLoaderInterface* g_loader_ref = nullptr;
AC2EaglePatch::Configuration g_config;
std::filesystem::path g_configPath;
static bool g_imgui_context_set = false;

class AC2EaglePatchPlugin : public IPlugin
{
public:
    const char* GetPluginName() override { return "AC2 EaglePatch"; }
    uint32_t GetPluginVersion() override { return MAKE_PLUGIN_API_VERSION(1, 0); }

    void OnPluginInit(const PluginLoaderInterface& loader_interface) override
    {
        g_loader_ref = &loader_interface;
        
        // Configure logging
        Log::InitSink(g_loader_ref->LogToConsole);

        LOG_INFO("[AC2 EaglePatch] Initializing...");

        uintptr_t baseAddr = (uintptr_t)GetModuleHandleA(NULL);
        auto version = AC2::DetectVersion(baseAddr);

        // --- Load Configuration ---
        g_configPath = PluginConfig::Load(g_config, (const void*)PluginEntry);

        if (version != AC2EaglePatch::GameVersion::Unknown)
        {
            if (g_config.EnableXInput)
                AC2EaglePatch::InitController(baseAddr, version, g_config.KeyboardLayout);

            if (g_config.SkipIntroVideos)
                AC2EaglePatch::InitSkipIntro(baseAddr, version);

            AC2EaglePatch::InitFPSUnlock(baseAddr, version, g_config.UnlockFPS);
            AC2EaglePatch::InitGraphics(baseAddr, version, g_config.ImproveShadowMapResolution, g_config.ImproveDrawDistance);

            if (g_config.UPlayItems)
                AC2EaglePatch::InitUplayBonus(baseAddr, version);

            if (g_config.FixCpuAffinity)
            {
                uint64_t mask = CpuAffinity::GetSystemAffinityMask() & ~1ULL;
                CpuAffinity::Apply(mask);
            }
        }
        else
            LOG_INFO("[AC2 EaglePatch] Unknown Game Version!");
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

        if (ImGui::CollapsingHeader("Realtime Patches", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (ImGui::Checkbox("Unlock FPS", &g_config.UnlockFPS.get()))
            {
                AC2EaglePatch::SetFPSUnlock(g_config.UnlockFPS);
            }

            if (ImGui::Checkbox("Improve Draw Distance", &g_config.ImproveDrawDistance.get()))
            {
                AC2EaglePatch::SetDrawDistance(g_config.ImproveDrawDistance);
            }

            if (ImGui::Checkbox("Improve Shadow Map Resolution", &g_config.ImproveShadowMapResolution.get()))
            {
                AC2EaglePatch::SetShadowMapResolution(g_config.ImproveShadowMapResolution);
            }
        }

        ImGui::Spacing();

        if (ImGui::CollapsingHeader("Requires Restart", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Checkbox("Hybrid Input (Simultaneous KB/M + Controller)", &g_config.EnableXInput.get());
            ImGui::Checkbox("Skip Intro Videos", &g_config.SkipIntroVideos.get());
            ImGui::Checkbox("Unlock UPlay Bonuses", &g_config.UPlayItems.get());
            if (ImGui::Checkbox("Fix CPU Affinity (Disable Core 0)", &g_config.FixCpuAffinity.get()))
            {
                if (g_config.FixCpuAffinity)
                    CpuAffinity::Apply(CpuAffinity::GetSystemAffinityMask() & ~1ULL);
                else
                    CpuAffinity::Apply(CpuAffinity::GetSystemAffinityMask());
            }

            const char* layouts[] = { "Keyboard / Mouse (2 buttons)", "Keyboard / Mouse (5 buttons)", "Keyboard", "Keyboard (Alt)" };
            int currentLayout = g_config.KeyboardLayout;
            if (currentLayout < 0 || currentLayout > 3) currentLayout = 0;
            if (ImGui::Combo("Keyboard Layout", &currentLayout, layouts, IM_ARRAYSIZE(layouts)))
            {
                g_config.KeyboardLayout = currentLayout;
                AC2EaglePatch::UpdateKeyboardLayout(currentLayout);
            }
        }

        ImGui::Separator();
        if (ImGui::Button("Save Settings"))
        {
            Serialization::JSON outJson;
            g_config.SectionToJSON(outJson);
            if (Serialization::Utils::SaveJSONToFile(outJson, g_configPath))
                LOG_INFO("[AC2 EaglePatch] Config saved.");
        }
        ImGui::TextDisabled("Note: 'Requires Restart' settings do not apply instantly.");
    }
};

extern "C" __declspec(dllexport) IPlugin* PluginEntry()
{
    return new AC2EaglePatchPlugin();
}
