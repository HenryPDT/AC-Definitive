#include <windows.h>
#include "IPlugin.h"
#include "imgui.h"
#include "EaglePatch.h"
#include "Patches/Controller.h"
#include "Patches/SkipIntro.h"
#include "Patches/Telemetry.h"
#include "Patches/Graphics.h"
#include <Serialization/Utils/FileSystem.h>
#include <filesystem>

// Define the global loader reference here
const PluginLoaderInterface* g_loader_ref = nullptr;
AC1EaglePatch::Configuration g_config;
static bool g_imgui_context_set = false;

class AC1EaglePatchPlugin : public IPlugin
{
public:
    const char* GetPluginName() override { return "AC1 EaglePatch (Controller Support)"; }
    uint32_t GetPluginVersion() override { return MAKE_PLUGIN_API_VERSION(1, 0); }

    void OnPluginInit(const PluginLoaderInterface& loader_interface) override
    {
        g_loader_ref = &loader_interface;
        g_loader_ref->LogToFile("[AC1 EaglePatch] Initializing...");

        uintptr_t baseAddr = (uintptr_t)GetModuleHandleA(NULL);
        auto version = AC1EaglePatch::DetectVersion(baseAddr);

        // --- Load Configuration ---
        HMODULE hModule = NULL;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)PluginEntry, &hModule);
        char modulePath[MAX_PATH];
        GetModuleFileNameA(hModule, modulePath, MAX_PATH);
        std::filesystem::path configPath = std::filesystem::path(modulePath).replace_extension(".json");

        Serialization::JSON jsonConfig = Serialization::Utils::LoadJSONFromFile(configPath);
        if (!jsonConfig.IsNull()) g_config.SectionFromJSON(jsonConfig);

        Serialization::JSON outJson;
        g_config.SectionToJSON(outJson);
        Serialization::Utils::SaveJSONToFile(outJson, configPath);

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
    }
};

extern "C" __declspec(dllexport) IPlugin* PluginEntry()
{
    return new AC1EaglePatchPlugin();
}
