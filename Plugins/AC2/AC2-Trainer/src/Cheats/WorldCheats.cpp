#include "Cheats/WorldCheats.h"
#include "Game/Singletons.h"
#include "Game/Managers/MapManager.h"
#include "Game/Managers/TimeOfDayManager.h"
#include "Game/Managers/MissionTimer.h"
#include "Core/Constants.h"
#include "Trainer.h"
#include "Hooks.h"
#include "imgui.h"

extern Trainer::Configuration g_config;

void WorldCheats::DrawUI()
{
    ImGui::Text("Time of Day");
    ImGui::Separator();
    
    // Time control
    auto* todManager = AC2::GetTimeOfDayManager();
    float* pCurrentTime = AC2::GetCurrentTimeGlobal();

    if (pCurrentTime)
    {
        // Read current time if we aren't dragging/interacting
        if (!ImGui::IsAnyItemActive() && !g_config.PauseTime)
            g_config.TimeOfDay = *pCurrentTime;

        if (ImGui::SliderFloat("Hour", &g_config.TimeOfDay.get(), 0.0f, 24.0f, "%.2f"))
        {
            *pCurrentTime = g_config.TimeOfDay;
        }

        if (todManager)
        {
        if (ImGui::Checkbox("Pause Time", &g_config.PauseTime.get()))
        {
                // CE: 0 = paused, 1 = unpaused (byte at [pDayTimeMgr]+58)
                todManager->m_IsPaused = g_config.PauseTime ? 0 : 1;
        }

            // Time Speed - writes to g_TimeScale global that the hook reads from
            // Slider: 1.0x = normal, 2.0x = faster (halves delay), 0.5x = slower (doubles delay)
            static float uiTimeScale = 1.0f;
            if (ImGui::SliderFloat("Time Flow Speed", &uiTimeScale, 0.1f, 100.0f, "%.1fx"))
            {
                Hooks::SetTimeScale(AC2::Constants::TIME_DELAY_DEFAULT / uiTimeScale);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Adjusts day/night cycle speed. Default: 1.0x");
        }
        else
        {
            ImGui::TextDisabled("Manager not found (Pause/Speed unavailable).");
        }
    }
    else
    {
        ImGui::TextDisabled("Time Global not available.");
    }

    ImGui::Separator();
    ImGui::Text("Mission Timer");

    if (AC2::GetMissionTimer())
    {
        ImGui::Checkbox("Freeze Mission Timer", &g_config.FreezeMissionTimer.get());
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Locks the countdown timer in races/courier missions.");
    }
    else
    {
        ImGui::TextDisabled("Timer not active.");
    }

    ImGui::Dummy(ImVec2(0, 10));
    ImGui::Text("Map Management");
    ImGui::Separator();

    // NOTE: MapManager hook in Hooks.cpp captures the WAYPOINT object, not the Manager.
    // The Manager pointer for flags might need a separate scan or verify Roots.
    // Roots.MapManager is 8B 49 18... which is getting waypoint.
    // Flags are at [pMapManage]+24.
    // CE Script 93339 hooks `94B0B8` to get `pMapManage`.
    // We didn't hook that yet. For now disabling Map Mode UI if pointer invalid.
    
    // Implementation requires MapManagerHook2 if we want to change map flags.
    // Current MapManagerHook is for Teleport.
    ImGui::TextDisabled("Map Flags not fully implemented.");
}

void WorldCheats::Update()
{
    // Skip all updates while in loading screen to prevent crash
    if (AC2::IsInWhiteRoom()) return;

    // Time Pause: continuously enforce pause state (0 = paused, 1 = unpaused)
    auto* todManager = AC2::GetTimeOfDayManager();
    if (todManager)
    {
        todManager->m_IsPaused = g_config.PauseTime ? 0 : 1;
    }

    // Mission Timer Freeze
    if (g_config.FreezeMissionTimer)
    {
        auto* timer = AC2::GetMissionTimer();
        if (timer && timer->m_pStartTime)
        {
            timer->m_SyncTime = *timer->m_pStartTime;
        }
    }
}