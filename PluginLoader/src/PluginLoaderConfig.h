#pragma once

#include <windows.h>
#include "Serialization/Config.h"
#include "Serialization/Adapters/NumericAdapters.h"
#include "Serialization/Adapters/EnumAdapter.h"
#include "Serialization/Adapters/HexAdapter.h"
#include "KeyBind.h"
#include <filesystem>
namespace fs = std::filesystem;

namespace PluginLoaderConfig {

    enum class ImGuiMouseInputSource : int
    {
        Win32 = 0,
        DirectInput = 1,
    };

    struct Config : Serialization::ConfigSection
    {
        SECTION_CTOR(Config);

        PROPERTY(hotkey_ToggleMenu, KeyBind, Serialization::KeyBindAdapter, KeyBind(VK_INSERT));
        PROPERTY(hotkey_ToggleConsole, KeyBind, Serialization::KeyBindAdapter, KeyBind(VK_OEM_3)); // Tilde
        PROPERTY(fontSize, float, Serialization::NumericAdapter_template<float>, 13.0f);

        // CPU Settings
        PROPERTY(CpuAffinityMask, uint64_t, Serialization::HexStringAdapter, 0);

        // DirectX Settings (Removed for rebase)
        // PROPERTY(DirectXVersion, DirectXVersion, Serialization::NumericEnumAdapter_template<DirectXVersion>, DirectXVersion::Auto);

        // Windowed Mode Settings (Removed for rebase)
        // PROPERTY(WindowedMode, WindowedMode, Serialization::NumericEnumAdapter_template<WindowedMode>, WindowedMode::ExclusiveFullscreen);
        // PROPERTY(ResizeBehavior, ResizeBehavior, Serialization::NumericEnumAdapter_template<ResizeBehavior>, ResizeBehavior::ResizeWindow);
        // PROPERTY(WindowPosX, int, Serialization::NumericAdapter_template<int>, -1); // -1 = center
        // PROPERTY(WindowPosY, int, Serialization::NumericAdapter_template<int>, -1);
        // PROPERTY(WindowWidth, int, Serialization::NumericAdapter_template<int>, 1920);
        // PROPERTY(WindowHeight, int, Serialization::NumericAdapter_template<int>, 1080);

        // Overlay mouse *buttons/wheel* routing (overlay only).
        // - Keyboard + mouse movement for overlay stays Win32/WndProc always.
        // - Game input remains split (WndProc movement/keys, DirectInput clicks/wheel).
        PROPERTY(ImGuiMouseSource, ImGuiMouseInputSource, Serialization::NumericEnumAdapter_template<ImGuiMouseInputSource>, ImGuiMouseInputSource::Win32);
    };

    extern Config g_Config;
    extern fs::path g_ConfigFilepath;

    void Init(HMODULE hModule);
    void Load();
    void Save();
    void CheckHotReload();
}