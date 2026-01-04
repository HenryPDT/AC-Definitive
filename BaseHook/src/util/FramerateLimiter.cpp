#include "pch.h"
#include "util/FramerateLimiter.h"
#include "log.h"
#include "core/BaseHook.h" // For Data::pContext11
#include <algorithm>
#include <cmath>
#include <numeric>
#include <timeapi.h>
#pragma comment(lib, "winmm.lib")

// Define missing constants if building on older SDKs
#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

namespace BaseHook {

    FramerateLimiter g_FramerateLimiter;

    FramerateLimiter::FramerateLimiter()
    {
        m_history.resize(HISTORY_SIZE, 0.0);
    }

    FramerateLimiter::~FramerateLimiter()
    {
        if (m_waitTimer) CloseHandle(m_waitTimer);
        timeEndPeriod(1);
    }

    void FramerateLimiter::Init()
    {
        QueryPerformanceFrequency(&m_qpcFreq);
        
        // Try to create high-resolution timer (Windows 10+)
        m_waitTimer = CreateWaitableTimerExW(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
        if (m_waitTimer) {
            m_hasHighResTimer = true;
            LOG_INFO("FramerateLimiter: High-resolution waitable timer created.");
        } else {
            m_waitTimer = CreateWaitableTimerW(NULL, FALSE, NULL);
            m_hasHighResTimer = false;
            LOG_WARN("FramerateLimiter: High-resolution timer not supported, falling back to legacy.");
        }

        // Ensure global timer resolution is granular if possible (fallback for non-waitable timer sleeps)
        timeBeginPeriod(1);

        // Recalculate target ticks now that we have frequency
        if (m_targetFPS > 0.0) {
            SetTargetFPS(m_targetFPS);
        }

        // Initialize start time
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);

        m_frameStartQPC = now.QuadPart;
        m_frameCount = 0;
        m_lastPresentTime = now.QuadPart;
    }

    void FramerateLimiter::SetTargetFPS(double fps)
    {
        std::lock_guard<std::mutex> lock(m_settingsMutex);
        m_targetFPS = fps;
        if (m_targetFPS < 10.0) m_targetFPS = 10.0; // Sanity clamp
        
        if (m_qpcFreq.QuadPart > 0) {
            double exactTicks = static_cast<double>(m_qpcFreq.QuadPart) / m_targetFPS;
            double integerPart;
            m_accumPerFrame = modf(exactTicks, &integerPart);
            m_ticksPerFrame = static_cast<LONGLONG>(integerPart);
            m_resetRequired = true;
        }
    }

    void FramerateLimiter::SetEnabled(bool enabled)
    {
        std::lock_guard<std::mutex> lock(m_settingsMutex);
        if (m_enabled != enabled) {
            m_enabled = enabled;
            m_resetRequired = true;
        }
    }

    void FramerateLimiter::UpdateStats(LONGLONG nowQPC)
    {
        if (m_lastPresentTime == 0) {
            m_lastPresentTime = nowQPC;
            return;
        }

        double ms = static_cast<double>(nowQPC - m_lastPresentTime) * 1000.0 / static_cast<double>(m_qpcFreq.QuadPart);
        m_lastPresentTime = nowQPC;

        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_history[m_historyIdx] = ms;
        m_historyIdx = (m_historyIdx + 1) % m_history.size();
    }

    void FramerateLimiter::Wait()
    {
        if (m_qpcFreq.QuadPart == 0) Init();

        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);

        UpdateStats(now.QuadPart);

        LONGLONG targetQPC = 0;
        LONGLONG ticksPerFrame = 0;

        {
            std::lock_guard<std::mutex> lock(m_settingsMutex);

            if (m_resetRequired) {
                m_frameStartQPC = now.QuadPart;
                m_frameCount = 0;
                m_resetRequired = false;
            }

            if (!m_enabled || m_ticksPerFrame <= 0) {
                // Keep the baseline updated so enabling doesn't cause a jump
                m_frameStartQPC = now.QuadPart;
                m_frameCount = 0;
                return;
            }

            m_frameCount++;
            ticksPerFrame = m_ticksPerFrame;

            // Calculate absolute target time for this frame
            // Target = Start + (Frames * Ticks) + (Frames * Accum)
            targetQPC = m_frameStartQPC + (m_frameCount * m_ticksPerFrame) + static_cast<LONGLONG>(m_frameCount * m_accumPerFrame);
        }

        LONGLONG timeToWait = targetQPC - now.QuadPart;
        
        // If we are significantly behind (more than 1 frame), reset target to avoid fast-forwarding
        // This handles loading screens, stalls, or just being too slow to render.
        if (timeToWait < -ticksPerFrame) {
            std::lock_guard<std::mutex> lock(m_settingsMutex);
            m_frameStartQPC = now.QuadPart;
            m_frameCount = 0;
            timeToWait = 0;
        }

        if (timeToWait > 0)
        {
            // Flush D3D11 context to allow GPU to work while CPU sleeps
            if (Data::pContext11) {
                Data::pContext11->Flush();
            }

            double timeToWaitMs = static_cast<double>(timeToWait) * 1000.0 / static_cast<double>(m_qpcFreq.QuadPart);

            // Boost priority for precise wake-up
            int oldPriority = GetThreadPriority(GetCurrentThread());
            SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
            
            // If wait is substantial (> 2ms), use waitable timer to save CPU
            if (m_waitTimer && timeToWaitMs > 2.0)
            {
                // Wake up slightly early (1ms buffer) to spin for precision
                LONGLONG sleepTicks = timeToWait - (m_qpcFreq.QuadPart / 1000); 
                if (sleepTicks > 0) {
                    LARGE_INTEGER dueTime;
                    // WaitableTimer uses 100ns units. Negative = relative.
                    // 1s = 10,000,000 * 100ns
                    LONGLONG wait100ns = (sleepTicks * 10000000) / m_qpcFreq.QuadPart;
                    dueTime.QuadPart = -wait100ns;

                    SetWaitableTimerEx(m_waitTimer, &dueTime, 0, NULL, NULL, NULL, 0);
                    WaitForSingleObject(m_waitTimer, INFINITE);
                }
            }

            // Busy wait for the remainder
            while (true)
            {
                QueryPerformanceCounter(&now);
                if (now.QuadPart >= targetQPC)
                    break;
                YieldProcessor();
            }

            SetThreadPriority(GetCurrentThread(), oldPriority);
        }
    }

    void FramerateLimiter::RecalculateStats() const
    {
        // Copy history in chronological order (Oldest -> Newest)
        std::vector<double> orderedHistory;
        orderedHistory.reserve(HISTORY_SIZE);

        {
            std::lock_guard<std::mutex> lock(m_statsMutex);
            // m_historyIdx points to the next write position (oldest element / overwriting point)
            // So the loop starting at m_historyIdx gives us oldest -> newest
            for(size_t i = 0; i < m_history.size(); ++i) {
                double val = m_history[(m_historyIdx + i) % m_history.size()];
                if (val > 0.0001) orderedHistory.push_back(val);
            }
        }

        if (orderedHistory.empty()) {
            m_cachedStats = {};
            return;
        }

        // --- Current Frametime ---
        m_cachedStats.frametime = orderedHistory.back();

        // --- Current FPS (Average over last 500ms) ---
        double sumTime = 0.0;
        int countCurrent = 0;
        // Iterate backwards from newest
        for (auto it = orderedHistory.rbegin(); it != orderedHistory.rend(); ++it) {
            sumTime += *it;
            countCurrent++;
            if (sumTime >= 500.0) break; // 500ms window
        }
        m_cachedStats.currentFps = (sumTime > 0.0) ? (1000.0 / (sumTime / countCurrent)) : 0.0;

        // --- Average FPS (Whole Buffer) ---
        double sumAvg = 0.0;
        for (double val : orderedHistory) sumAvg += val;
        double avgFrametime = sumAvg / orderedHistory.size();
        m_cachedStats.avgFps = (avgFrametime > 0.0) ? (1000.0 / avgFrametime) : 0.0;

        // --- Percentile Lows ---
        // Sort descending (Highest Frametime = Lowest FPS)
        // We can reuse the vector since we don't need order anymore
        std::sort(orderedHistory.begin(), orderedHistory.end(), std::greater<double>());

        auto CalcLow = [&](double percent) -> double {
            size_t count = orderedHistory.size();
            size_t numSamples = std::max<size_t>(1, static_cast<size_t>(std::ceil(count * (percent / 100.0))));

            double sumLow = 0.0;
            for(size_t i = 0; i < numSamples && i < count; ++i) {
                sumLow += orderedHistory[i];
            }
            double avgLowMs = sumLow / numSamples;
            return (avgLowMs > 0.0) ? (1000.0 / avgLowMs) : 0.0;
        };

        m_cachedStats.low1 = CalcLow(1.0);
        m_cachedStats.low01 = CalcLow(0.1);
    }

    FrameStats FramerateLimiter::GetStats() const
    {
        if (m_qpcFreq.QuadPart == 0) return {};

        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);

        // Update cache at most 4 times per second to save CPU on sorting
        if (now.QuadPart - m_lastStatsUpdate > (m_qpcFreq.QuadPart / 4)) {
            RecalculateStats();
            m_lastStatsUpdate = now.QuadPart;
        }
        
        return m_cachedStats;
    }
}