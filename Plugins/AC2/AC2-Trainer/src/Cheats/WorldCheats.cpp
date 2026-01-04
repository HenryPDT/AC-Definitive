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
    if (AC2::IsInWhiteRoom())
    {
        ImGui::TextDisabled("Unavailable while loading/in white room");
        return;
    }

    // Time of Day Section
    if (ImGui::CollapsingHeader("Time of Day", ImGuiTreeNodeFlags_DefaultOpen))
    {
        auto* todManager = reinterpret_cast<AC2::TimeOfDayManager*>(Hooks::GetDayTimeMgrPointer());
        float* pCurrentTime = AC2::GetCurrentTimeGlobal();

        if (pCurrentTime)
        {
            // Read current time if we aren't dragging/interacting
            if (!ImGui::IsAnyItemActive() && !g_config.PauseTime)
                g_config.TimeOfDay = *pCurrentTime;

            if (ImGui::DragFloat("Hour", &g_config.TimeOfDay.get(), 0.01f, 0.0f, 0.0f, "%.2f"))
            {
                // Wrap hours bidirectionally: 24+ wraps to 0+, negative wraps to 24-
                float hour = g_config.TimeOfDay;
                while (hour >= 24.0f) hour -= 24.0f;
                while (hour < 0.0f) hour += 24.0f;
                g_config.TimeOfDay = hour;
                *pCurrentTime = hour;
            }

            if (todManager)
            {
                if (ImGui::Checkbox("Pause Time", &g_config.PauseTime.get()))
                {
                    todManager->m_IsPaused = g_config.PauseTime ? 0 : 1;
                }

                static float uiTimeScale = 1.0f;
                ImGui::DragFloat("Time Speed", &uiTimeScale, 0.1f, 0.1f, 1000.0f, "%.1fx");
                if (ImGui::IsItemEdited())
                {
                    Hooks::SetTimeScale(AC2::Constants::TIME_DELAY_DEFAULT / uiTimeScale);
                }
                ImGui::SameLine();
                if (ImGui::Button("Reset##TimeSpeed"))
                {
                    uiTimeScale = 1.0f;
                    Hooks::SetTimeScale(AC2::Constants::TIME_DELAY_DEFAULT);
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Adjusts day/night cycle speed. Default: 1.0x");
            }
            else
            {
                ImGui::TextDisabled("Manager not captured. Play for a moment.");
            }
        }
        else
        {
            ImGui::TextDisabled("Time Global not available.");
        }
    }

    // Mission Timer Section
    if (ImGui::CollapsingHeader("Mission Timer"))
    {
        if (AC2::GetMissionTimer())
        {
            ImGui::Checkbox("Freeze Mission Timer", &g_config.FreezeMissionTimer.get());
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Locks countdown timer in races/courier missions.");
        }
        else
        {
            ImGui::TextDisabled("Timer not active.");
        }
    }

    // Map Management Section
    if (ImGui::CollapsingHeader("Map Management"))
    {
        void* pMapManage = Hooks::GetMapManagePointer();
        if (pMapManage)
        {
            uint8_t* pMapFlags = reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(pMapManage) + 0x24);
            
            static const char* mapFlagModes[] = { 
                "Disabled", 
                "All Icons (In Progress)", 
                "Cleared Map (In Progress)", 
                "All Icons (Cleared Map)" 
            };
            
            int currentMode = *pMapFlags;
            if (currentMode < 0 || currentMode > 3) currentMode = 0;
            
            if (ImGui::Combo("Map Flags Mode", &currentMode, mapFlagModes, 4))
            {
                *pMapFlags = static_cast<uint8_t>(currentMode);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Affects how map icons and collectibles are shown.");
        }
        else
        {
            ImGui::TextDisabled("Map pointer not captured. Play for a moment.");
        }
    }
}

void WorldCheats::Update()
{
    // Skip all updates while in loading screen to prevent crash
    if (AC2::IsInWhiteRoom()) return;

    // Mission Timer Freeze (uses hook-captured pointer which is safer)
    if (g_config.FreezeMissionTimer)
    {
        auto* timer = AC2::GetMissionTimer();
        if (timer && timer->m_pStartTime)
        {
            timer->m_SyncTime = *timer->m_pStartTime;
        }
    }
}