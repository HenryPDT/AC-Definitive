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
    DrawPlayerStatus();
}

void PlayerCheats::DrawPlayerStatus()
{
    auto* health = AC2::GetPlayerHealth();
    int32_t currHealth = 0, maxHealth = 0, armorDmg = 0;

    if (health)
    {
        currHealth = health->m_CurrentHealth;
        maxHealth = health->m_MaxHealth;
        armorDmg = health->m_ArmorDamage;
    }

    // Health Section
    if (health)
    {
        ImGui::Text("Health: %d / %d", currHealth, maxHealth);
        ImGui::Text("Armor Damage: %d", armorDmg);
    }
    else
    {
        ImGui::TextDisabled("Health pointer not found.");
    }

    ImGui::Checkbox("God Mode", &g_config.GodMode.get());
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Sets internal God Mode flag (0x81).");

    ImGui::Checkbox("Lock Consumables (Infinite Items)", &g_config.InfiniteItems.get());
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Locks inventory items (including Knives) to max.");

    ImGui::Checkbox("Ignore Fall Damage", &g_config.IgnoreFallDamage.get());
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Prevents death from falling (Refills Health + Resets Armor Damage).");

    // Other Status
    ImGui::Separator();
    ImGui::TextDisabled("-- Status --");
    
    ImGui::Checkbox("Invisible", &g_config.Invisible.get());
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Enemies will not detect you.");

    ImGui::Checkbox("Disable Notoriety", &g_config.DisableNotoriety.get());
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Locks Notoriety to zero.");

    // Notoriety Value - use captured pNotoriety pointer from hook
    // CE stores notoriety at [pNotoriety+0x0C]
    void* pNotoriety = Hooks::GetNotorietyPointer();
    if (pNotoriety && !g_config.DisableNotoriety)
    {
        float* pNotorietyValue = reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(pNotoriety) + 0x0C);
        float notoriety = *pNotorietyValue;
        if (ImGui::SliderFloat("Notoriety Value (0.0 - 1.0)", &notoriety, 0.0f, 1.0f, "%.2f"))
        {
            *pNotorietyValue = notoriety;
        }
    }
    else if (!pNotoriety)
    {
        ImGui::TextDisabled("Notoriety pointer not captured yet.");
    }
}

void PlayerCheats::DrawMiscUI()
{
    // Resize Player
    ImGui::TextDisabled("-- Resize Player --");
    if (ImGui::SliderFloat("Scale (0.5 - 2.0)", &g_config.PlayerScale.get(), 0.5f, 2.0f, "%.2f"))
    {
        // Applied in Update
    }
    if (ImGui::Button("Reset Scale")) g_config.PlayerScale = 1.0f;

    ImGui::Separator();

    // Speed Player
    ImGui::TextDisabled("-- Speed Player --");
    ImGui::TextWrapped("Controls Global Speed System (affects animation and movement).");
    ImGui::SliderFloat("Speed Multiplier", &g_config.MovementSpeed.get(), 0.1f, 3.0f, "%.2f");
    if (ImGui::Button("Reset Speed")) g_config.MovementSpeed = 1.0f;
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
    // NotorietyHook handles this via xmm0 zeroing; backup write for redundancy
    auto* bhv = AC2::GetBhvAssassin();
    if (bhv && bhv->m_pCharacterAI && g_config.DisableNotoriety)
    {
        bhv->m_pCharacterAI->m_Notoriety = 0.0f;
    }
}

void PlayerCheats::UpdateSpeed()
{
    // Speed handled via Hooks::Update() and SpeedPlayerHook.
    // We update the Global Speed System just in case it's used elsewhere,
    // but the actual player movement multiplication is now hooked.

    auto* sys = AC2::GetSpeedSystem();
    if (sys)
    {
        sys->m_IsEnabled = (g_config.MovementSpeed != 1.0f);
        sys->m_GlobalMultiplier = g_config.MovementSpeed;
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