#include <windows.h>
#include "IPlugin.h"
#include "imgui.h"
#include "EaglePatch.h"
#include "Patches/Controller.h"
#include "Patches/SkipIntro.h"
#include "Patches/Telemetry.h"

// Define the global loader reference here
const PluginLoaderInterface* g_loader_ref = nullptr;
static bool g_imgui_context_set = false;

class AC1EaglePatchPlugin : public IPlugin
{
public:
    const char* GetPluginName() override { return "AC1 EaglePatch (Controller Support)"; }
    uint32_t GetPluginVersion() override { return MAKE_PLUGIN_API_VERSION(1, 0); }

    void OnPluginInit(const PluginLoaderInterface& loader_interface) override
    {
        g_loader_ref = &loader_interface;
        g_loader_ref->LogToFile("[EaglePatch] Initializing...");

        uintptr_t baseAddr = (uintptr_t)GetModuleHandleA(NULL);
        auto version = AC1EaglePatch::DetectVersion(baseAddr);

        if (version != AC1EaglePatch::GameVersion::Unknown)
        {
            AC1EaglePatch::InitController(baseAddr, version);
            AC1EaglePatch::InitSkipIntro(baseAddr, version);
            AC1EaglePatch::InitTelemetry(baseAddr, version);
        }
        else
            g_loader_ref->LogToConsole("[EaglePatch] Unknown Game Version!");
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
