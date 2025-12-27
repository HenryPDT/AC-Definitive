#include <windows.h>
#include "IPlugin.h"
#include "imgui.h"
#include "EaglePatch.h"
#include "Patches/Controller.h"
#include "Patches/SkipIntro.h"
#include "Patches/Telemetry.h"
#include "Patches/Graphics.h"
#include <PluginConfig.h>
#include <filesystem>

// Define the global loader reference here
const PluginLoaderInterface* g_loader_ref = nullptr;
AC1EaglePatch::Configuration g_config;
std::filesystem::path g_configPath;
static bool g_imgui_context_set = false;

class AC1EaglePatchPlugin : public IPlugin
{
public:
    const char* GetPluginName() override { return "AC1 EaglePatch"; }
    uint32_t GetPluginVersion() override { return MAKE_PLUGIN_API_VERSION(1, 0); }

    void OnPluginInit(const PluginLoaderInterface& loader_interface) override
    {
        g_loader_ref = &loader_interface;
        g_loader_ref->LogToFile("[AC1 EaglePatch] Initializing...");

        uintptr_t baseAddr = (uintptr_t)GetModuleHandleA(NULL);
        auto version = AC1EaglePatch::DetectVersion(baseAddr);

        // --- Load Configuration ---
        g_configPath = PluginConfig::Load(g_config, (const void*)PluginEntry);

        if (version != AC1EaglePatch::GameVersion::Unknown)
        {
            if (g_config.EnableXInput)
                AC1EaglePatch::InitController(baseAddr, version, g_config.KeyboardLayout);

            if (g_config.SkipIntroVideos)
                AC1EaglePatch::InitSkipIntro(baseAddr, version);

            AC1EaglePatch::InitGraphics(baseAddr, version, g_config.MultisamplingFix, g_config.D3D10_RemoveDuplicateResolutions);

            if (g_config.DisableTelemetry)
                AC1EaglePatch::InitTelemetry(baseAddr, version);
        }
        else
            g_loader_ref->LogToConsole("[AC1 EaglePatch] Unknown Game Version!");
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

        ImGui::Checkbox("Enable Controller Support (XInput)", &g_config.EnableXInput.get());
        ImGui::Checkbox("Skip Intro Videos", &g_config.SkipIntroVideos.get());
        ImGui::Checkbox("Multisampling Fix (MSAA)", &g_config.MultisamplingFix.get());
        ImGui::Checkbox("D3D10 Duplicate Res Fix", &g_config.D3D10_RemoveDuplicateResolutions.get());
        ImGui::Checkbox("Disable Telemetry", &g_config.DisableTelemetry.get());

        const char* layouts[] = { "Keyboard/Mouse 1", "Keyboard/Mouse 2", "Keyboard/Mouse 3", "Keyboard/Mouse 4" };
        int currentLayout = g_config.KeyboardLayout;
        if (currentLayout < 0 || currentLayout > 3) currentLayout = 0;
        if (ImGui::Combo("Keyboard Layout", &currentLayout, layouts, IM_ARRAYSIZE(layouts)))
        {
            g_config.KeyboardLayout = currentLayout;
            AC1EaglePatch::UpdateKeyboardLayout(currentLayout);
        }

        ImGui::Separator();
        if (ImGui::Button("Save Settings"))
        {
            Serialization::JSON outJson;
            g_config.SectionToJSON(outJson);
            if (Serialization::Utils::SaveJSONToFile(outJson, g_configPath) && g_loader_ref)
                g_loader_ref->LogToConsole("[AC1 EaglePatch] Config saved.");
        }
        ImGui::TextDisabled("Note: Most settings require a restart.");
    }
};

extern "C" __declspec(dllexport) IPlugin* PluginEntry()
{
    return new AC1EaglePatchPlugin();
}
