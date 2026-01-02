#include <windows.h>
#include "IPlugin.h"
#include "imgui.h"

namespace
{
    const PluginLoaderInterface* g_loader = nullptr;
    bool g_imgui_context_set = false;
}

class ACBParkourMod : public IPlugin
{
public:
    const char* GetPluginName() override { return "ACB Parkour Mod"; }
    uint32_t GetPluginVersion() override { return MAKE_PLUGIN_API_VERSION(0, 1); }

    void OnPluginInit(const PluginLoaderInterface& loader_interface) override
    {
        g_loader = &loader_interface;
        g_loader->LogToFile("[ACB] Plugin Initialized!");
    }

    void OnGuiRender() override
    {
        if (!g_imgui_context_set && g_loader && g_loader->m_ImGuiContext)
        {
            ImGui::SetCurrentContext(g_loader->m_ImGuiContext);
            g_imgui_context_set = true;
        }
        if (!g_imgui_context_set) return;

        ImGui::Text("This is the menu for the ACB plugin.");
    }

    void OnUpdate() override
    {
        // Game logic for ACB will go here.
    }
};

extern "C" __declspec(dllexport) IPlugin* PluginEntry()
{
    return new ACBParkourMod();
}
