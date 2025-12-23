#pragma once

#include <cstdint>

#include "PluginLoaderConfig.h"
// #include "WindowedMode.h" // Removed for rebase
#include "KeyBind.h"

struct SettingsDraft
{
    // Windowed mode settings removed for rebase
    // int windowedMode = 0;
    // int resizeBehavior = 0;
    // int windowX = -1;
    // int windowY = -1;
    // int windowW = 1920;
    // int windowH = 1080;

    // CPU affinity
    uint64_t cpuAffinityMask = 0;

    // Hotkeys + appearance
    KeyBind toggleMenu;
    KeyBind toggleConsole;
    int fontSize = 13;
};

class SettingsModel
{
public:
    void LoadFromConfig();
    void SyncVirtualResolutionIfNeeded();

    // UI rendering and interactions
    // void DrawWindowedModeSection(); // Removed for rebase
    void DrawCpuAffinitySection();
    void DrawHotkeysSection();
    void DrawAppearanceSection();
    void DrawInputSection();

    bool DrawSaveRow(); // returns true if saved

private:
    // void ApplyWindowedModeToRuntime(bool allowFakeReset); // Removed for rebase
    void ApplyCpuAffinityToRuntime();

    SettingsDraft m_draft;
    // bool m_draggingWindow = false; // Removed for rebase
    uint64_t m_systemAffinityMask = 0;
};