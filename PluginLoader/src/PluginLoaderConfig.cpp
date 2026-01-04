#include "PluginLoaderConfig.h"
#include "Serialization/Serialization.h"
#include "Serialization/Utils/FileSystem.h"
#include "log.h"
#include <chrono>
#include "util/FramerateLimiter.h"

namespace PluginLoaderConfig {

    Config g_Config;
    fs::path g_ConfigFilepath;
    fs::file_time_type g_LastWriteTime;
    static fs::file_time_type g_LastObservedWriteTime;
    static bool g_HasPendingReload = false;
    static std::chrono::steady_clock::time_point g_PendingSince{};
    static constexpr auto kReloadDebounce = std::chrono::milliseconds(250);

    void Init(HMODULE hModule)
    {
        char modulePath[MAX_PATH];
        GetModuleFileNameA(hModule, modulePath, MAX_PATH);
        g_ConfigFilepath = std::filesystem::path(modulePath).replace_extension(".json");
    }

    void Load()
    {
        if (g_ConfigFilepath.empty()) return;
        if (fs::exists(g_ConfigFilepath))
        {
            std::error_code ec;
            const auto writeTime = fs::last_write_time(g_ConfigFilepath, ec);
            if (ec)
            {
                LOG_WARN("Config load: failed to stat config file (%s).", g_ConfigFilepath.string().c_str());
                return;
            }

            Serialization::JSON cfg = Serialization::Utils::LoadJSONFromFile(g_ConfigFilepath);
            if (cfg.IsNull())
            {
                LOG_WARN("Config load: JSON was null/invalid, keeping last-good config.");
                return;
            }

            // Apply atomically-ish: only advance timestamps after successful parse+apply.
            bool dirty = g_Config.SectionFromJSON(cfg);
            g_LastWriteTime = writeTime;
            g_LastObservedWriteTime = writeTime;
            g_HasPendingReload = false;

            // Apply runtime settings that aren't applied elsewhere (e.g. by SettingsModel)
            BaseHook::g_FramerateLimiter.SetEnabled(g_Config.EnableFPSLimit);
            BaseHook::g_FramerateLimiter.SetTargetFPS(static_cast<double>(g_Config.FPSLimit));

            LOG_INFO("Config loaded.");

            if (dirty)
            {
                LOG_INFO("Config load: Detected missing or invalid keys, updating file.");
                Save();
            }
        }
        else
        {
            Save();
        }
    }

    void Save()
    {
        if (g_ConfigFilepath.empty()) return;
        Serialization::JSON cfg;
        g_Config.SectionToJSON(cfg);
        Serialization::Utils::SaveJSONToFile(cfg, g_ConfigFilepath);
        
        std::error_code ec;
        if (fs::exists(g_ConfigFilepath, ec) && !ec)
        {
            g_LastWriteTime = fs::last_write_time(g_ConfigFilepath, ec);
            if (!ec)
                g_LastObservedWriteTime = g_LastWriteTime;
        }
    }

    void CheckHotReload()
    {
        if (g_ConfigFilepath.empty()) return;
        
        std::error_code ec;
        if (fs::exists(g_ConfigFilepath, ec))
        {
            const auto currentWriteTime = fs::last_write_time(g_ConfigFilepath, ec);
            if (ec) return;

            // Detect a new write and start (or restart) debounce window.
            if (currentWriteTime > g_LastObservedWriteTime)
            {
                g_LastObservedWriteTime = currentWriteTime;
                g_HasPendingReload = true;
                g_PendingSince = std::chrono::steady_clock::now();
                return;
            }

            if (g_HasPendingReload)
            {
                const auto now = std::chrono::steady_clock::now();
                if ((now - g_PendingSince) < kReloadDebounce)
                    return;

                // Only reload if file is still newer than last-good.
                if (g_LastObservedWriteTime > g_LastWriteTime)
                {
                    LOG_INFO("Config change detected on disk. Reloading...");
                    Load();
                }
                else
                {
                    g_HasPendingReload = false;
                }
            }
        }
    }
}