#pragma once
#include <Windows.h>
#include <cstdint>

// Forward declare ImGui context to avoid including imgui.h in this public header.
struct ImGuiContext;

#define MAKE_PLUGIN_API_VERSION(major, minor) ((major << 16) | minor)
constexpr uint32_t g_PluginLoaderAPIVersion = MAKE_PLUGIN_API_VERSION(1, 0);

// Game identifiers
enum class Game
{
    Unknown,
    AC1,
    AC2,
    ACB,
    ACR,
    AC3,
    AC4,
};

// Forward declaration
class PluginLoaderInterface;

struct ImGuiShared
{
    ImGuiContext& m_ctx;
    void* (*alloc_func)(size_t sz, void* user_data);
    void (*free_func)(void* ptr, void* user_data);
    void* user_data;
};

class IPlugin
{
public:
    virtual ~IPlugin() = default;
    virtual const char* GetPluginName() = 0;
    virtual uint32_t GetPluginVersion() = 0;
    virtual uint32_t GetPluginAPIVersion() { return g_PluginLoaderAPIVersion; }

    // Called when the loader has confirmed API compatibility.
    // Good place for one-time initializations like reading configs.
    virtual void OnPluginInit(const PluginLoaderInterface& loader_interface) = 0;

    // Called every frame when the menu is open to render UI widgets.
    // Do NOT create a window (ImGui::Begin/End) here; the loader handles containers.
    virtual void OnGuiRender() {}

    // Called every frame, regardless of menu state.
    virtual void OnUpdate() {}

    // Optional: Expose a pointer to an interface/controller for other plugins to use.
    virtual void* GetInterface() { return nullptr; }
};

// The interface the loader provides to plugins.
class PluginLoaderInterface
{
public:
    uint32_t m_LoaderAPIVersion = g_PluginLoaderAPIVersion;
    void (*RequestUnloadPlugin)(HMODULE pluginHandle) = nullptr;
    Game (*GetCurrentGame)() = nullptr;
    void (*LogToConsole)(const char* text) = nullptr;
    ImGuiContext* m_ImGuiContext = nullptr;
    ImGuiContext* (*GetImGuiContext)() = nullptr;
    void* (*GetPluginInterface)(const char* pluginName) = nullptr;
};

// Each plugin must export this function. It should return a new instance of your plugin's main class.
using PluginEntrypoint = IPlugin* (*)();
extern "C" __declspec(dllexport) IPlugin* PluginEntry();
