#include "Cheats/InventoryCheats.h"
#include "Game/Singletons.h"
#include "Game/Inventory.h"
#include "Trainer.h"
#include "imgui.h"

extern Trainer::Configuration g_config;

void InventoryCheats::DrawUI()
{
    if (AC2::IsInWhiteRoom())
    {
        ImGui::TextDisabled("Unavailable while loading/in white room");
        return;
    }

    auto* inv = AC2::GetInventory();

    // Money Section
    if (ImGui::CollapsingHeader("Money", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (inv && inv->m_pList && inv->m_pList->m_pMoney)
        {
            int money = inv->m_pList->m_pMoney->m_Count;
            if (ImGui::DragInt("Florins", &money, 100, 0, 9999999))
            {
                inv->m_pList->m_pMoney->m_Count = money;
            }
        }
        else
        {
            ImGui::TextDisabled("Money not available (load a save).");
        }
    }

    // Consumables Section
    if (ImGui::CollapsingHeader("Consumables", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::TextDisabled("Enable 'Infinite Items' on Player tab to lock all.");
        
        if (inv && inv->m_pList)
        {
            // Helper lambda for drawing items
            auto drawItem = [](const char* label, AC2::InventoryItem* item) {
                if (!item) return;
                int val = item->m_Count;
                if (ImGui::DragInt(label, &val, 1, 0, 99)) {
                    item->m_Count = val;
                }
            };

            drawItem("Medicine", inv->m_pList->m_pMedicine);
            drawItem("Smoke Bombs", inv->m_pList->m_pSmokeBombs);
            drawItem("Bullets", inv->m_pList->m_pBullets);
            drawItem("Poison", inv->m_pList->m_pPoison);

            // Knives (uint8_t - read-only display, modifying can crash)
            if (inv->m_pList->m_pKnives)
            {
                int val = (int)inv->m_pList->m_pKnives->m_CountByte;
                ImGui::Text("Knives: %d", val);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Knives are handled via hook only to prevent crashes.");
            }
        }
        else
        {
            ImGui::TextDisabled("Inventory not available (load a save).");
        }
    }
}

void InventoryCheats::Update()
{
    // Intentionally empty.
    // The ASM hooks (LockConsumables and LockKnives) handle infinite items by blocking
    // the decrement instruction at runtime. Direct value modification is NOT safe,
    // especially for knives which are container-managed and will crash if written to.
    // This matches the CE trainer approach exactly.
}