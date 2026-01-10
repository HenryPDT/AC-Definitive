#pragma once

#include <cstdint>

#include "PluginLoaderConfig.h"
#include "core/WindowedMode.h"
#include "KeyBind.h"

struct SettingsDraft
{
    // Windowed mode
    int windowedMode = 0;
    int resizeBehavior = 0;
    int windowX = -1;
    int windowY = -1;
    int windowW = 1920;
    int windowH = 1080;
    int targetMonitor = 0;
    int cursorClipMode = 0;
    int overrideResX = 0;
    int overrideResY = 0;
    bool alwaysOnTop = false;
    bool enableMultiViewport = false;
    int multiViewportScaling = 1; // BaseHook::WindowedMode::ViewportScalingMode::ScalePhysical

    // CPU affinity
    uint64_t cpuAffinityMask = 0;

    // FPS Limiter
    bool enableFpsLimit = false;
    int fpsLimit = 60;
    bool renderInBackground = false;

    // Hotkeys + appearance
    KeyBind toggleMenu;
    KeyBind toggleConsole;
    int fontSize = 13;
};

struct WindowSettingsSnapshot
{
    int windowedMode = 0;
    int resizeBehavior = 0;
    int windowX = -1;
    int windowY = -1;
    int windowW = 1920;
    int windowH = 1080;
    int targetMonitor = 0;
    int cursorClipMode = 0;
    int overrideResX = 0;
    int overrideResY = 0;
    bool alwaysOnTop = false;
    bool renderInBackground = false;
    bool enableMultiViewport = false;
    int multiViewportScaling = 1;
    bool enableFpsLimit = false;
    int fpsLimit = 0;
};

class SettingsModel
{
public:
    void LoadFromConfig();
    void SyncVirtualResolutionIfNeeded();
    void SyncDetectedDisplayStateIfNeeded();

    // UI rendering and interactions
    void DrawWindowedModeSection();
    void DrawCpuAffinitySection();
    void DrawHotkeysSection();
    void DrawAppearanceSection();
    void DrawFramerateSection();
    void DrawInputSection();

    bool DrawSaveRow(); // returns true if saved

private:
    void UpdateSnapshot();
    void ApplyWindowedModeToRuntime(bool allowFakeReset);
    void ApplyCpuAffinityToRuntime();
    void ApplyFramerateToRuntime();

    SettingsDraft m_draft;
    WindowSettingsSnapshot m_appliedWindowSettings;
    bool m_draggingWindow = false;
    int m_savedCursorClipMode = -1; // -1 = not saved
    uint64_t m_systemAffinityMask = 0;
    uint64_t m_lastAppliedAffinity = 0;

    // Runtime detected state (DXGI). -1 = unknown, 0 = not exclusive, 1 = exclusive.
    int m_detectedExclusive = -1;
};


