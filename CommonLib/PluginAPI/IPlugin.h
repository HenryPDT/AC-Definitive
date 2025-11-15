#pragma once
#include <Windows.h>
#include <cstdint>
#include <filesystem>
#include <string_view>

// Forward declare ImGui context to avoid including imgui.h in this public header.
struct ImGuiContext;

#define MAKE_PLUGIN_API_VERSION(major, minor) ((major << 16) | minor)
constexpr uint32_t g_PluginLoaderAPIVersion = MAKE_PLUGIN_API_VERSION(1, 2);

// Game identifiers
enum class Game
{
    Unknown,
    AC1,
    AC2,
    ACB,
    ACR,
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
    virtual std::string_view GetPluginName() = 0;
    virtual uint32_t GetPluginVersion() = 0;
    virtual uint32_t GetPluginAPIVersion() { return g_PluginLoaderAPIVersion; }

    // Called when the loader has confirmed API compatibility.
    // Good place for one-time initializations like reading configs.
    virtual void OnPluginInit(const PluginLoaderInterface& loader_interface) = 0;

    // Called every frame when the menu is open.
    virtual void OnGuiRender() {}

    // Called every frame, regardless of menu state.
    virtual void OnUpdate() {}
};

// The interface the loader provides to plugins.
class PluginLoaderInterface
{
public:
    uint32_t m_LoaderAPIVersion = g_PluginLoaderAPIVersion;
    void (*RequestUnloadPlugin)(HMODULE pluginHandle) = nullptr;
    Game (*GetCurrentGame)() = nullptr;
    void (*LogToConsole)(const char* text) = nullptr;
    void (*LogToFile)(const char* fmt, ...) = nullptr;
    ImGuiContext* m_ImGuiContext = nullptr;
};

// Each plugin must export this function. It should return a new instance of your plugin's main class.
using PluginEntrypoint = IPlugin* (*)();
extern "C" __declspec(dllexport) IPlugin* PluginEntry();