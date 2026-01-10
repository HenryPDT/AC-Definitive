#pragma once

#include <windows.h>
#include "Serialization/Config.h"
#include "Serialization/Adapters/NumericAdapters.h"
#include "Serialization/Adapters/EnumAdapter.h"
#include "Serialization/Adapters/HexAdapter.h"
#include "KeyBind.h"
#include "core/BaseHook.h"
#include "core/WindowedMode.h"
#include <filesystem>
namespace fs = std::filesystem;

namespace PluginLoaderConfig {

    using WindowedMode = ::BaseHook::WindowedMode::Mode;
    using ResizeBehavior = ::BaseHook::WindowedMode::ResizeBehavior;
    using DirectXVersion = ::BaseHook::DirectXVersion;
    using CursorClipMode = ::BaseHook::WindowedMode::CursorClipMode;
    using ViewportScalingMode = ::BaseHook::WindowedMode::ViewportScalingMode;

    enum class ImGuiMouseInputSource : int
    {
        DirectInput = 0,
        Win32 = 1,
    };

    struct Config : Serialization::ConfigSection
    {
        SECTION_CTOR(Config);

        PROPERTY(hotkey_ToggleMenu, KeyBind, Serialization::KeyBindAdapter, KeyBind(VK_INSERT));
        PROPERTY(hotkey_ToggleConsole, KeyBind, Serialization::KeyBindAdapter, KeyBind(VK_OEM_3)); // Tilde
        PROPERTY(fontSize, int, Serialization::IntegerAdapter_template<int>, 20);

        // CPU Settings
        PROPERTY(CpuAffinityMask, uint64_t, Serialization::HexStringAdapter, 0);

        // DirectX Settings
        PROPERTY(DirectXVersion, DirectXVersion, Serialization::NumericEnumAdapter_template<DirectXVersion>, DirectXVersion::Auto);

        // Windowed Mode Settings
        PROPERTY(WindowedMode, WindowedMode, Serialization::NumericEnumAdapter_template<WindowedMode>, WindowedMode::BorderlessFullscreen);
        PROPERTY(ResizeBehavior, ResizeBehavior, Serialization::NumericEnumAdapter_template<ResizeBehavior>, ResizeBehavior::ResizeWindow);
        PROPERTY(WindowPosX, int, Serialization::IntegerAdapter_template<int>, -1); // -1 = center
        PROPERTY(WindowPosY, int, Serialization::IntegerAdapter_template<int>, -1);
        PROPERTY(WindowWidth, int, Serialization::IntegerAdapter_template<int>, 1920);
        PROPERTY(WindowHeight, int, Serialization::IntegerAdapter_template<int>, 1080);
        PROPERTY(TargetMonitor, int, Serialization::IntegerAdapter_template<int>, -1);
        PROPERTY(CursorClipMode, CursorClipMode, Serialization::NumericEnumAdapter_template<CursorClipMode>, CursorClipMode::Default);
        PROPERTY(OverrideResX, int, Serialization::IntegerAdapter_template<int>, 0);
        PROPERTY(OverrideResY, int, Serialization::IntegerAdapter_template<int>, 0);
        PROPERTY(AlwaysOnTop, bool, Serialization::BooleanAdapter, false);
        PROPERTY(RenderInBackground, bool, Serialization::BooleanAdapter, false);
        PROPERTY(EnableMultiViewport, bool, Serialization::BooleanAdapter, false);
        PROPERTY(MultiViewportScaling, ViewportScalingMode, Serialization::NumericEnumAdapter_template<ViewportScalingMode>, ViewportScalingMode::ScalePhysical);

        // Framerate Limiter
        PROPERTY(EnableFPSLimit, bool, Serialization::BooleanAdapter, false);
        PROPERTY(FPSLimit, int, Serialization::NumericAdapter_template<int>, 60);


        // Overlay mouse *buttons/wheel* routing (overlay only).
        // - Keyboard + mouse movement for overlay stays Win32/WndProc always.
        // - Game input remains split (WndProc movement/keys, DirectInput clicks/wheel).
        PROPERTY(ImGuiMouseSource, ImGuiMouseInputSource, Serialization::NumericEnumAdapter_template<ImGuiMouseInputSource>, ImGuiMouseInputSource::DirectInput);
    };

    extern Config g_Config;
    extern fs::path g_ConfigFilepath;

    void Init(HMODULE hModule);
    void Load();
    void Save();
    void CheckHotReload();
}
