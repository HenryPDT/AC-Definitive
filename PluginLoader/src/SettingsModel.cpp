#include "SettingsModel.h"
#include "CpuAffinity.h"
#include "ImGuiConfigUtils.h"
#include "log.h"
#include "util/FramerateLimiter.h"
#include "core/BaseHook.h"

#include "imgui.h"

namespace
{
    bool IsExclusiveFullscreen(int mode)
    {
        return mode == (int)BaseHook::WindowedMode::Mode::ExclusiveFullscreen;
    }
}

void SettingsModel::LoadFromConfig()
{
    m_draft.windowedMode = (int)PluginLoaderConfig::g_Config.WindowedMode.get();
    m_draft.resizeBehavior = (int)PluginLoaderConfig::g_Config.ResizeBehavior.get();
    m_draft.windowW = PluginLoaderConfig::g_Config.WindowWidth.get();
    m_draft.windowH = PluginLoaderConfig::g_Config.WindowHeight.get();
    m_draft.windowX = PluginLoaderConfig::g_Config.WindowPosX.get();
    m_draft.windowY = PluginLoaderConfig::g_Config.WindowPosY.get();
    m_draft.cursorClipMode = (int)PluginLoaderConfig::g_Config.CursorClipMode.get();
    m_draft.overrideResX = PluginLoaderConfig::g_Config.OverrideResX.get();
    m_draft.overrideResY = PluginLoaderConfig::g_Config.OverrideResY.get();
    m_draft.alwaysOnTop = PluginLoaderConfig::g_Config.AlwaysOnTop.get();

    m_draft.targetMonitor = PluginLoaderConfig::g_Config.TargetMonitor.get();
    if (m_draft.targetMonitor == -1)
    {
        m_draft.targetMonitor = BaseHook::WindowedMode::GetPrimaryMonitorIndex();
    }

    m_draft.cpuAffinityMask = PluginLoaderConfig::g_Config.CpuAffinityMask.get();
    m_systemAffinityMask = CpuAffinity::GetSystemAffinityMask();

    m_draft.enableFpsLimit = PluginLoaderConfig::g_Config.EnableFPSLimit.get();
    m_draft.fpsLimit = PluginLoaderConfig::g_Config.FPSLimit.get();
    m_draft.renderInBackground = PluginLoaderConfig::g_Config.RenderInBackground.get();

    m_draft.toggleMenu = PluginLoaderConfig::g_Config.hotkey_ToggleMenu.get();
    m_draft.toggleConsole = PluginLoaderConfig::g_Config.hotkey_ToggleConsole.get();
    m_draft.fontSize = PluginLoaderConfig::g_Config.fontSize.get();

    UpdateSnapshot();
}

void SettingsModel::UpdateSnapshot()
{
    m_appliedWindowSettings.windowedMode = m_draft.windowedMode;
    m_appliedWindowSettings.resizeBehavior = m_draft.resizeBehavior;
    m_appliedWindowSettings.windowX = m_draft.windowX;
    m_appliedWindowSettings.windowY = m_draft.windowY;
    m_appliedWindowSettings.windowW = m_draft.windowW;
    m_appliedWindowSettings.windowH = m_draft.windowH;
    m_appliedWindowSettings.targetMonitor = m_draft.targetMonitor;
    m_appliedWindowSettings.cursorClipMode = m_draft.cursorClipMode;
    m_appliedWindowSettings.overrideResX = m_draft.overrideResX;
    m_appliedWindowSettings.overrideResY = m_draft.overrideResY;
    m_appliedWindowSettings.alwaysOnTop = m_draft.alwaysOnTop;
    m_appliedWindowSettings.enableFpsLimit = m_draft.enableFpsLimit;
    m_appliedWindowSettings.fpsLimit = m_draft.fpsLimit;
    m_appliedWindowSettings.renderInBackground = m_draft.renderInBackground;
}

void SettingsModel::DrawInputSection()
{
    const char* items[] = { "Win32/WndProc", "DirectInput" };
    int mode = (int)PluginLoaderConfig::g_Config.ImGuiMouseSource.get();
    if (mode < 0 || mode > 1) mode = 0;
    if (ImGui::Combo("ImGui Mouse Source", &mode, items, IM_ARRAYSIZE(items)))
    {
        PluginLoaderConfig::g_Config.ImGuiMouseSource = (PluginLoaderConfig::ImGuiMouseInputSource)mode;
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip(
            "Controls how the overlay (ImGui) receives mouse buttons + wheel.\n"
            "This is mainly for troubleshooting.\n"
            "Game input remains split:\n"
            "- Keyboard + mouse movement via WndProc\n"
            "- Mouse buttons + wheel via DirectInput"
        );
    }
    
    ImGui::Separator();
    ImGui::Checkbox("Fix Button Mappings (DirectInput)", &BaseHook::Data::bFixDirectInput);
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip(
            "Swaps buttons to match Xbox layout on generic DirectInput controllers:\n"
            "- A <-> X\n"
            "- B <-> Y\n"
            "- LB <-> LT\n"
            "- RB <-> RT\n\n"
            "Note: This option has no effect if 'Hybrid Input' is enabled in the EaglePatch patch,\n"
            "as that replaces the input system entirely with XInput."
        );
    }

    ImGui::Dummy(ImVec2(0, 5));
    if (ImGui::Button("Reset to Default##Input"))
    {
        PluginLoaderConfig::g_Config.ImGuiMouseSource = PluginLoaderConfig::Config().ImGuiMouseSource.get();
        BaseHook::Data::bFixDirectInput = false; 
    }
}

void SettingsModel::SyncVirtualResolutionIfNeeded()
{
    // If resize behavior is "match game resolution" (ResizeWindow), reflect current virtual resolution in the UI.
    if (m_draft.resizeBehavior == (int)BaseHook::WindowedMode::ResizeBehavior::ResizeWindow)
    {
        int vw = 0, vh = 0;
        BaseHook::WindowedMode::GetVirtualResolution(vw, vh);
        if (vw > 0 && vh > 0)
        {
            m_draft.windowW = vw;
            m_draft.windowH = vh;
        }
    }

    if (m_draggingWindow)
    {
        m_draft.windowX = BaseHook::WindowedMode::g_State.windowX;
        m_draft.windowY = BaseHook::WindowedMode::g_State.windowY;
        m_draft.targetMonitor = BaseHook::WindowedMode::g_State.targetMonitor;
    }
}

void SettingsModel::SyncDetectedDisplayStateIfNeeded()
{
    // DXGI only: shows whether the swapchain is in exclusive fullscreen right now.
    if (BaseHook::WindowedMode::HasDetectedFullscreenState())
        m_detectedExclusive = BaseHook::WindowedMode::IsDetectedExclusiveFullscreen() ? 1 : 0;
    else
        m_detectedExclusive = -1;
}

void SettingsModel::ApplyWindowedModeToRuntime(bool allowFakeReset)
{
    const bool modeChanged =
        ((BaseHook::WindowedMode::g_State.activeMode == BaseHook::WindowedMode::Mode::ExclusiveFullscreen) !=
         (IsExclusiveFullscreen(m_draft.windowedMode)));

    const bool resChanged =
        (m_draft.overrideResX != m_appliedWindowSettings.overrideResX) ||
        (m_draft.overrideResY != m_appliedWindowSettings.overrideResY);
    const bool topChanged = (m_draft.alwaysOnTop != m_appliedWindowSettings.alwaysOnTop);

    const bool needsFakeReset = modeChanged || resChanged || topChanged;

    PluginLoaderConfig::g_Config.WindowedMode = (PluginLoaderConfig::WindowedMode)m_draft.windowedMode;
    PluginLoaderConfig::g_Config.ResizeBehavior = (PluginLoaderConfig::ResizeBehavior)m_draft.resizeBehavior;
    PluginLoaderConfig::g_Config.WindowWidth = m_draft.windowW;
    PluginLoaderConfig::g_Config.WindowHeight = m_draft.windowH;
    PluginLoaderConfig::g_Config.WindowPosX = m_draft.windowX;
    PluginLoaderConfig::g_Config.WindowPosY = m_draft.windowY;
    PluginLoaderConfig::g_Config.TargetMonitor = m_draft.targetMonitor;
    PluginLoaderConfig::g_Config.CursorClipMode = (PluginLoaderConfig::CursorClipMode)m_draft.cursorClipMode;
    PluginLoaderConfig::g_Config.OverrideResX = m_draft.overrideResX;
    PluginLoaderConfig::g_Config.OverrideResY = m_draft.overrideResY;
    PluginLoaderConfig::g_Config.AlwaysOnTop = m_draft.alwaysOnTop;

    BaseHook::WindowedMode::SetSettings(
        (BaseHook::WindowedMode::Mode)m_draft.windowedMode,
        (BaseHook::WindowedMode::ResizeBehavior)m_draft.resizeBehavior,
        m_draft.windowX, m_draft.windowY, m_draft.windowW, m_draft.windowH, m_draft.targetMonitor,
        m_draft.cursorClipMode, m_draft.overrideResX, m_draft.overrideResY, m_draft.alwaysOnTop);

    if (allowFakeReset && needsFakeReset)
        BaseHook::WindowedMode::TriggerFakeReset();

    UpdateSnapshot();
}

void SettingsModel::ApplyCpuAffinityToRuntime()
{
    if (m_draft.cpuAffinityMask == 0)
        m_draft.cpuAffinityMask = (m_systemAffinityMask ? m_systemAffinityMask : 1ULL);

    PluginLoaderConfig::g_Config.CpuAffinityMask = m_draft.cpuAffinityMask;
    
    // Only apply if it differs from current state to avoid redundant syscalls
    if (m_draft.cpuAffinityMask != CpuAffinity::GetCurrentProcessMask())
    {
        CpuAffinity::Apply(m_draft.cpuAffinityMask);
    }
    m_lastAppliedAffinity = m_draft.cpuAffinityMask;
}

void SettingsModel::ApplyFramerateToRuntime()
{
    PluginLoaderConfig::g_Config.EnableFPSLimit = m_draft.enableFpsLimit;
    PluginLoaderConfig::g_Config.FPSLimit = m_draft.fpsLimit;
    PluginLoaderConfig::g_Config.RenderInBackground = m_draft.renderInBackground;
    BaseHook::g_FramerateLimiter.SetEnabled(m_draft.enableFpsLimit);
    BaseHook::g_FramerateLimiter.SetTargetFPS(static_cast<double>(m_draft.fpsLimit));
}

void SettingsModel::DrawWindowedModeSection()
{
    // Runtime detection (read-only)
    {
        const char* detected =
            (m_detectedExclusive < 0) ? "Unknown" :
            (m_detectedExclusive == 1) ? "Exclusive Fullscreen" : "Windowed (Not Exclusive)";

        ImGui::Text("Detected: %s", detected);

        int vw = 0, vh = 0;
        BaseHook::WindowedMode::GetVirtualResolution(vw, vh);
        ImGui::Text("Ingame Res: %dx%d", vw, vh);

        int winW = 0, winH = 0;
        BaseHook::WindowedMode::GetWindowResolution(winW, winH);
        ImGui::Text("Window Res: %dx%d", winW, winH);

        // Highlight mismatch (useful during Alt+Tab before focus returns).
        const bool configuredExclusive = IsExclusiveFullscreen(m_draft.windowedMode);
        if (m_detectedExclusive >= 0 && configuredExclusive != (m_detectedExclusive == 1))
        {
            ImGui::TextColored(ImVec4(1, 0.75f, 0.2f, 1), "Configured: %s",
                configuredExclusive ? "Exclusive Fullscreen" : "Windowed");
        }
    }

    // Remap internal modes to UI options:
    // 0: Exclusive Fullscreen
    // 1: Borderless Fullscreen
    // 2: Windowed (Bordered/Borderless)
    int currentScreenMode = 0;
    if (IsExclusiveFullscreen(m_draft.windowedMode)) currentScreenMode = 0;
    else if (m_draft.windowedMode == (int)BaseHook::WindowedMode::Mode::BorderlessFullscreen) currentScreenMode = 1;
    else currentScreenMode = 2;

    const char* screenModes[] = { "Exclusive Fullscreen", "Borderless Fullscreen", "Windowed" };
    if (ImGui::Combo("Screen Mode", &currentScreenMode, screenModes, IM_ARRAYSIZE(screenModes)))
    {
        if (currentScreenMode == 0) m_draft.windowedMode = (int)BaseHook::WindowedMode::Mode::ExclusiveFullscreen;
        else if (currentScreenMode == 1) m_draft.windowedMode = (int)BaseHook::WindowedMode::Mode::BorderlessFullscreen;
        else m_draft.windowedMode = (int)BaseHook::WindowedMode::Mode::Bordered; // Default to Bordered for Windowed
    }

    // Monitor Selector
    if (currentScreenMode != 0) // Not Exclusive Fullscreen
    {
        const auto& monitors = BaseHook::WindowedMode::GetMonitors();
        if (!monitors.empty())
        {
            if (m_draft.targetMonitor < 0 || m_draft.targetMonitor >= (int)monitors.size())
                m_draft.targetMonitor = 0;

            if (ImGui::BeginCombo("Target Monitor", monitors[m_draft.targetMonitor].name.c_str()))
            {
                for (int i = 0; i < (int)monitors.size(); i++)
                {
                    const bool isSelected = (m_draft.targetMonitor == i);
                    if (ImGui::Selectable(monitors[i].name.c_str(), isSelected))
                    {
                        m_draft.targetMonitor = i;
                        // Reset manual positioning to center on new monitor
                        m_draft.windowX = -1;
                        m_draft.windowY = -1;
                    }
                    if (isSelected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        }
        else
        {
            ImGui::TextDisabled("No monitors detected.");
        }
    }

    ImGui::Dummy(ImVec2(0, 5));
    ImGui::Text("Resolution Override (0 = Disabled)");
    ImGui::InputInt("Override Width", &m_draft.overrideResX);
    ImGui::InputInt("Override Height", &m_draft.overrideResY);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Forces the game to render at this resolution.\nUseful for Downsampling (e.g. 4k rendering on 1080p screen)\nor Upsampling with 'Scale Content' behavior.");

    // Options applicable to Windowed/Borderless only
    if (currentScreenMode != 0) // Hide if Exclusive Fullscreen
    {
        if (ImGui::Checkbox("Continue Rendering in Background", &m_draft.renderInBackground))
            PluginLoaderConfig::g_Config.RenderInBackground = m_draft.renderInBackground;

        ImGui::SameLine();
        ImGui::Checkbox("Always On Top", &m_draft.alwaysOnTop);
    }

    if (currentScreenMode == 1) // Borderless Fullscreen
    {
        ImGui::TextDisabled("Window fills the screen. Game content scales to fit.");
        // Enforce ScaleContent behavior logic for UI consistency, though runtime ignores it for positioning
        m_draft.resizeBehavior = (int)BaseHook::WindowedMode::ResizeBehavior::ScaleContent;
    }
    else if (currentScreenMode == 2) // Windowed
    {
        const char* windowStyles[] = { "Borderless", "Bordered" };
        // internal modes: Borderless=2, Bordered=3. Map 0->2, 1->3
        int currentWindowStyle = (m_draft.windowedMode == (int)BaseHook::WindowedMode::Mode::Borderless) ? 0 : 1;

        if (ImGui::Combo("Window Style", &currentWindowStyle, windowStyles, IM_ARRAYSIZE(windowStyles)))
            m_draft.windowedMode = (currentWindowStyle == 0) ? (int)BaseHook::WindowedMode::Mode::Borderless : (int)BaseHook::WindowedMode::Mode::Bordered;

        const char* resizeModes[] = { "Match Game Resolution", "Scale Content to Window" };
        ImGui::Combo("Resize Behavior", &m_draft.resizeBehavior, resizeModes, IM_ARRAYSIZE(resizeModes));
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Match: Window automatically resizes to match game resolution.\nScale: Window size is fixed; game image scales to fit.");

        ImGui::BeginDisabled(m_draft.resizeBehavior == (int)BaseHook::WindowedMode::ResizeBehavior::ResizeWindow);
        ImGui::InputInt("Window Width", &m_draft.windowW);
        ImGui::InputInt("Window Height", &m_draft.windowH);
        ImGui::EndDisabled();

        ImGui::InputInt("Pos X (-1=Center)", &m_draft.windowX);
        ImGui::InputInt("Pos Y (-1=Center)", &m_draft.windowY);

        if (ImGui::Button("Hold to Drag Window")) {}
        BaseHook::WindowedMode::HandleDrag(ImGui::IsItemActive());
        m_draggingWindow = ImGui::IsItemActive();
        if (m_draggingWindow)
        {
            m_draft.windowX = BaseHook::WindowedMode::g_State.windowX;
            m_draft.windowY = BaseHook::WindowedMode::g_State.windowY;
            ImGui::SetTooltip("Dragging: %d, %d", m_draft.windowX, m_draft.windowY);
        }

        ImGui::SameLine();
        if (ImGui::Button("Center"))
        {
            m_draft.windowX = -1;
            m_draft.windowY = -1;
            ApplyWindowedModeToRuntime(true);
        }
    }

    ImGui::Dummy(ImVec2(0, 5));
    
            const char* clipModes[] = {
                "Default",
                "Force Clip",
                "Force Unlock",
                "Unlock on Menu"
            };
            ImGui::Combo("Cursor Clipping", &m_draft.cursorClipMode, clipModes, IM_ARRAYSIZE(clipModes));
    if (ImGui::IsItemHovered()) ImGui::SetTooltip(
        "Default: Game controls cursor clipping.\n"
        "Force Clip: Cursor is trapped inside the window.\n"
        "Force Unlock: Cursor can always leave the window.\n"
        "Unlock on Menu: Unlocked when menu is open, clipped otherwise."
    );

    // Check for pending changes
    bool changed =
        m_draft.windowedMode != m_appliedWindowSettings.windowedMode ||
        m_draft.resizeBehavior != m_appliedWindowSettings.resizeBehavior ||
        m_draft.windowX != m_appliedWindowSettings.windowX ||
        m_draft.windowY != m_appliedWindowSettings.windowY ||
        m_draft.windowW != m_appliedWindowSettings.windowW ||
        m_draft.windowH != m_appliedWindowSettings.windowH ||
        m_draft.targetMonitor != m_appliedWindowSettings.targetMonitor ||
        m_draft.cursorClipMode != m_appliedWindowSettings.cursorClipMode ||
        m_draft.overrideResX != m_appliedWindowSettings.overrideResX ||
        m_draft.overrideResY != m_appliedWindowSettings.overrideResY ||
        m_draft.renderInBackground != m_appliedWindowSettings.renderInBackground ||
        m_draft.alwaysOnTop != m_appliedWindowSettings.alwaysOnTop;

    ImGui::Dummy(ImVec2(0, 10));
    if (changed)
    {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Settings changed. Press Apply to take effect.");
    }

    if (ImGui::Button("Apply Changes"))
    {
        ApplyWindowedModeToRuntime(true);
        PluginLoaderConfig::Save();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset to Default##Window"))
    {
        PluginLoaderConfig::Config defaults;
        m_draft.windowedMode = (int)defaults.WindowedMode.get();
        m_draft.resizeBehavior = (int)defaults.ResizeBehavior.get();
        m_draft.windowW = defaults.WindowWidth.get();
        m_draft.windowH = defaults.WindowHeight.get();
        m_draft.windowX = defaults.WindowPosX.get();
        m_draft.windowY = defaults.WindowPosY.get();
        m_draft.targetMonitor = BaseHook::WindowedMode::GetPrimaryMonitorIndex();
        m_draft.cursorClipMode = (int)defaults.CursorClipMode.get();
        m_draft.overrideResX = defaults.OverrideResX.get();
        m_draft.overrideResY = defaults.OverrideResY.get();
        m_draft.alwaysOnTop = defaults.AlwaysOnTop.get();
        m_draft.renderInBackground = defaults.RenderInBackground.get();
    }
}

void SettingsModel::DrawFramerateSection()
{
    ImGui::Text("Stats:");
    auto stats = BaseHook::g_FramerateLimiter.GetStats();

    if (ImGui::BeginTable("FramerateStatsTable", 2, ImGuiTableFlags_None))
    {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Current FPS: %.1f", stats.currentFps);
        ImGui::TableNextColumn();
        ImGui::Text("Avg FPS: %.1f", stats.avgFps);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("1%% Low: %.1f", stats.low1);
        ImGui::TableNextColumn();
        ImGui::Text("0.1%% Low: %.1f", stats.low01);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Frametime: %.2f ms", stats.frametime);

        ImGui::EndTable();
    }

    ImGui::Separator();

    if (ImGui::Checkbox("Enable FPS Limiter", &m_draft.enableFpsLimit)) {
        ApplyFramerateToRuntime();
    }

    ImGui::BeginDisabled(!m_draft.enableFpsLimit);
    int minFPS = 10, maxFPS = 240;
    if (ImGui::DragInt("Target FPS", &m_draft.fpsLimit, 1.0f, minFPS, maxFPS)) {
        ApplyFramerateToRuntime();
    }
    ImGui::EndDisabled();

    if (ImGui::Button("Reset to Default##Framerate"))
    {
        PluginLoaderConfig::Config defaults;
        m_draft.enableFpsLimit = defaults.EnableFPSLimit.get();
        m_draft.fpsLimit = defaults.FPSLimit.get();
        ApplyFramerateToRuntime();
    }
}

void SettingsModel::DrawCpuAffinitySection()
{
    if (m_systemAffinityMask == 0)
        m_systemAffinityMask = CpuAffinity::GetSystemAffinityMask();

    // Sync with actual process affinity if it changed externally (e.g. by a plugin)
    uint64_t currentMask = CpuAffinity::GetCurrentProcessMask();
    if (currentMask != 0 && currentMask != m_lastAppliedAffinity)
    {
        m_draft.cpuAffinityMask = currentMask;
        m_lastAppliedAffinity = currentMask;
    }

    // --- Presets Row 1 ---
    if (ImGui::Button("Default (All)"))
    {
        m_draft.cpuAffinityMask = m_systemAffinityMask;
        ApplyCpuAffinityToRuntime();
    }
    ImGui::SameLine();
    if (ImGui::Button("Disable SMT"))
    {
        m_draft.cpuAffinityMask = m_systemAffinityMask & 0x5555555555555555ULL;
        ApplyCpuAffinityToRuntime();
    }

    // --- Presets Row 2 ---
    if (ImGui::Button("Limit to 31 Cores"))
    {
        m_draft.cpuAffinityMask = m_systemAffinityMask & 0x7FFFFFFFULL;
        ApplyCpuAffinityToRuntime();
    }
    ImGui::SameLine();
    if (ImGui::Button("Disable Core 0"))
    {
        m_draft.cpuAffinityMask = m_systemAffinityMask & ~1ULL;
        ApplyCpuAffinityToRuntime();
    }

    ImGui::Dummy(ImVec2(0, 5));
    ImGui::Text("Manual Core Selection:");

    if (ImGui::BeginChild("CpuList", ImVec2(0, 150), true, ImGuiWindowFlags_None))
    {
        ImGui::Columns(2, "CpuCols", false);
        for (int i = 0; i < 64; ++i)
        {
            if (!((m_systemAffinityMask >> i) & 1ULL))
                break;

            char label[32];
            snprintf(label, sizeof(label), "CPU %d", i);

            bool enabled = ((m_draft.cpuAffinityMask >> i) & 1ULL) != 0;
            if (ImGui::Checkbox(label, &enabled))
            {
                if (enabled) m_draft.cpuAffinityMask |= (1ULL << i);
                else m_draft.cpuAffinityMask &= ~(1ULL << i);

                if (m_draft.cpuAffinityMask == 0)
                    m_draft.cpuAffinityMask = (1ULL << i);

                ApplyCpuAffinityToRuntime();
            }
            ImGui::NextColumn();
        }
        ImGui::Columns(1);
    }
    ImGui::EndChild();
}

void SettingsModel::DrawHotkeysSection()
{
    if (ImGui::KeyBindInput("Toggle Menu", m_draft.toggleMenu))
        PluginLoaderConfig::g_Config.hotkey_ToggleMenu = m_draft.toggleMenu;
    if (ImGui::KeyBindInput("Toggle Console", m_draft.toggleConsole))
        PluginLoaderConfig::g_Config.hotkey_ToggleConsole = m_draft.toggleConsole;

    if (ImGui::Button("Reset to Default##Hotkeys"))
    {
        PluginLoaderConfig::Config defaults;
        m_draft.toggleMenu = defaults.hotkey_ToggleMenu.get();
        m_draft.toggleConsole = defaults.hotkey_ToggleConsole.get();
        PluginLoaderConfig::g_Config.hotkey_ToggleMenu = m_draft.toggleMenu;
        PluginLoaderConfig::g_Config.hotkey_ToggleConsole = m_draft.toggleConsole;
    }
}

void SettingsModel::DrawAppearanceSection()
{
    if (ImGui::DragInt("Font Size", &m_draft.fontSize, 1, 8, 32))
        PluginLoaderConfig::g_Config.fontSize = m_draft.fontSize;

    if (ImGui::Button("Reset to Default##Appearance"))
    {
        PluginLoaderConfig::Config defaults;
        m_draft.fontSize = defaults.fontSize.get();
        PluginLoaderConfig::g_Config.fontSize = m_draft.fontSize;
    }
}

bool SettingsModel::DrawSaveRow()
{
    bool saved = false;
    if (ImGui::Button("Save Config"))
    {
        PluginLoaderConfig::Save();
        saved = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Save Window Layout (imgui.ini)"))
        ImGui::SaveIniSettingsToDisk(ImGui::GetIO().IniFilename);

    static bool show_demo = false;
    if (ImGui::Button("Show ImGui Demo Window"))
        show_demo = !show_demo;
    if (show_demo)
        ImGui::ShowDemoWindow(&show_demo);

    return saved;
}


