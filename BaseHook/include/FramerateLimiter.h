#pragma once
#include <Windows.h>
#include <vector>
#include <mutex>
#include <atomic>

namespace BaseHook {

    struct FrameStats {
        double currentFps = 0.0; // Smoothed over last ~500ms
        double avgFps = 0.0;     // Rolling average over history buffer
        double frametime = 0.0;
        double low1 = 0.0;
        double low01 = 0.0;
    };

    class FramerateLimiter {
    public:
        FramerateLimiter();
        ~FramerateLimiter();

        void Init();
        
        // Configuration
        void SetTargetFPS(double fps);
        void SetEnabled(bool enabled);
        bool IsEnabled() const { return m_enabled; }
        double GetTargetFPS() const { return m_targetFPS; }

        // Core - Must be called just before Present
        void Wait();

        // Stats (Thread Safe)
        FrameStats GetStats() const;

    private:
        // Timer
        LARGE_INTEGER m_qpcFreq = {};
        LONGLONG m_ticksPerFrame = 0;
        double m_accumPerFrame = 0.0;

        // Absolute timing tracking to prevent drift
        LONGLONG m_frameStartQPC = 0;
        LONGLONG m_frameCount = 0;
        
        // Settings
        bool m_enabled = false;
        double m_targetFPS = 60.0;

        // Waitable Timer
        HANDLE m_waitTimer = NULL;
        bool m_hasHighResTimer = false;

        // Stats
        static const size_t HISTORY_SIZE = 1000;
        std::vector<double> m_history;
        size_t m_historyIdx = 0;
        LONGLONG m_lastPresentTime = 0;
        mutable std::mutex m_statsMutex;
        mutable std::mutex m_settingsMutex;
        bool m_resetRequired = false;

        // Cached stats to avoid re-sorting every frame for UI
        mutable FrameStats m_cachedStats;
        mutable LONGLONG m_lastStatsUpdate = 0;

        // Helpers
        void UpdateStats(LONGLONG nowQPC);
        void RecalculateStats() const;
    };

    extern FramerateLimiter g_FramerateLimiter;
}