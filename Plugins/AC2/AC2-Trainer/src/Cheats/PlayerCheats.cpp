#include "Cheats/PlayerCheats.h"
#include "Game/Singletons.h"
#include "Game/Entity.h"
#include "Game/BhvAssassin.h"
#include "Game/SharedData.h"
#include "Game/Components/BipedComponent.h"
#include "Game/SpeedSystem.h"
#include "Game/Managers/CSrvPlayerHealth.h"
#include "Core/Constants.h"
#include "Trainer.h"
#include "Hooks.h"
#include "imgui.h"

extern Trainer::Configuration g_config;

void PlayerCheats::DrawUI()
{
    // Safety check - if not in game, don't try to read pointers that might be invalid/changing
    if (AC2::IsInWhiteRoom())
    {
        ImGui::TextDisabled("Unavailable while loading/in white room");
        return;
    }

    DrawPlayerStatus();
}

void PlayerCheats::DrawPlayerStatus()
{
    // Health & Cheats Section
    if (ImGui::CollapsingHeader("Health & Cheats", ImGuiTreeNodeFlags_DefaultOpen))
    {
        auto* health = AC2::GetPlayerHealth();
        
        if (health)
        {
            ImGui::Text("Health: %d / %d", health->m_CurrentHealth, health->m_MaxHealth);
            ImGui::Text("Armor Damage: %d", health->m_ArmorDamage);
        }
        else
        {
            ImGui::TextDisabled("Health pointer not found.");
        }

        ImGui::Spacing();
        
        ImGui::Checkbox("God Mode", &g_config.GodMode.get());
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Sets internal God Mode flag (0x81).");

        ImGui::Checkbox("Infinite Items", &g_config.InfiniteItems.get());
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Locks consumables (including Knives) to max.");

        ImGui::Checkbox("Ignore Fall Damage", &g_config.IgnoreFallDamage.get());
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Prevents death from falling.");

        ImGui::Spacing();
        if (ImGui::Button("Desynchronize (Kill Player)"))
        {
            auto* health = AC2::GetPlayerHealth();
            if (health) health->m_Desync = true;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Instantly desynchronizes the player to reload checkpoint.");
    }

    // Status Section
    if (ImGui::CollapsingHeader("Status Effects", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Invisible", &g_config.Invisible.get());
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Enemies will not detect you.");

        ImGui::Checkbox("Disable Notoriety", &g_config.DisableNotoriety.get());
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Locks Notoriety to zero.");

        // Notoriety Value - use captured pNotoriety pointer from hook
        void* pNotoriety = Hooks::GetNotorietyPointer();
        if (pNotoriety && !g_config.DisableNotoriety)
        {
            float* pNotorietyValue = reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(pNotoriety) + 0x0C);
            if (!IsBadReadPtr(pNotorietyValue, sizeof(float)))
            {
                float notoriety = *pNotorietyValue;
                if (ImGui::SliderFloat("Notoriety", &notoriety, 0.0f, 1.0f, "%.2f"))
                {
                    *pNotorietyValue = notoriety;
                }
            }
        }
        else if (!pNotoriety)
        {
            ImGui::TextDisabled("Notoriety pointer not captured yet.");
        }
    }
}

void PlayerCheats::DrawMiscUI()
{
    // Scale Section
    if (ImGui::CollapsingHeader("Player Scale", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::DragFloat("Scale", &g_config.PlayerScale.get(), 0.01f, 0.25f, 4.0f, "%.2f");
        ImGui::SameLine();
        if (ImGui::Button("Reset##Scale")) g_config.PlayerScale = 1.0f;
    }

    // Speed Section
    if (ImGui::CollapsingHeader("Speed Settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::TextDisabled("Player animation/movement speed");
        ImGui::DragFloat("Player Speed", &g_config.PlayerSpeed.get(), 0.01f, 0.1f, 10.0f, "%.2f");
        ImGui::SameLine();
        if (ImGui::Button("Reset##PlayerSpeed")) g_config.PlayerSpeed = 1.0f;
        
        ImGui::Spacing();
        
        ImGui::TextDisabled("Global game speed (affects everything)");
        ImGui::DragFloat("Global Speed", &g_config.GlobalSpeed.get(), 0.01f, 0.1f, 10.0f, "%.2f");
        ImGui::SameLine();
        if (ImGui::Button("Reset##GlobalSpeed")) g_config.GlobalSpeed = 1.0f;
    }
}

void PlayerCheats::Update()
{
    // Skip updates while in loading screen to prevent crash
    if (AC2::IsInWhiteRoom()) return;

    UpdateGodMode();
    UpdateInvisibility();
    UpdateNotoriety();
    UpdateSpeed();
    UpdateScale();
    UpdateMiscAndFallDamage();
}

void PlayerCheats::UpdateGodMode()
{
    auto* sharedData = AC2::GetSharedData();
    if (!sharedData) return;
    
    uint8_t desired = g_config.GodMode ? AC2::Constants::GOD_MODE_ON : AC2::Constants::GOD_MODE_OFF;
    if (sharedData->m_GodModeFlags != desired)
        sharedData->m_GodModeFlags = desired;
}

void PlayerCheats::UpdateInvisibility()
{
    auto* bhv = AC2::GetBhvAssassin();
    if (!bhv) return;
    
    bhv->m_bIsInvisible = g_config.Invisible;
}

void PlayerCheats::UpdateNotoriety()
{
    // NotorietyHook handles notoriety zeroing via xmm0.
    // No additional work needed here - the hook intercepts all notoriety reads.
}

void PlayerCheats::UpdateSpeed()
{
    // PlayerSpeed is handled via Hooks::Update() -> SpeedPlayerHook (BipedComponent)
    // GlobalSpeed controls the SpeedSystem (global game speed)

    auto* sys = AC2::GetSpeedSystem();
    if (sys)
    {
        sys->m_IsEnabled = (g_config.GlobalSpeed != 1.0f);
        sys->m_GlobalMultiplier = g_config.GlobalSpeed;
    }
}

void PlayerCheats::UpdateScale()
{
    auto* player = AC2::GetPlayer();
    if (player)
    {
        if (player->Scale != g_config.PlayerScale) 
            player->Scale = g_config.PlayerScale;
        
        if (player->m_pVisual && player->m_pVisual->m_Scale != g_config.PlayerScale) 
            player->m_pVisual->m_Scale = g_config.PlayerScale;
    }
}

void PlayerCheats::UpdateMiscAndFallDamage()
{
    if (g_config.IgnoreFallDamage)
    {
        auto* health = AC2::GetPlayerHealth();
        if (health)
        {
            if (health->m_CurrentHealth < health->m_MaxHealth)
                health->m_CurrentHealth = health->m_MaxHealth;
            
            health->m_ArmorDamage = 0;
        }
    }
}