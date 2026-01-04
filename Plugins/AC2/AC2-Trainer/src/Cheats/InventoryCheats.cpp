#include "Cheats/InventoryCheats.h"
#include "Game/Singletons.h"
#include "Game/Inventory.h"
#include "Trainer.h"
#include "imgui.h"

extern Trainer::Configuration g_config;

void InventoryCheats::DrawUI()
{
    ImGui::Text("Inventory Management");
    ImGui::TextWrapped("Edit values directly below. Enable 'Infinite Items' to lock them to max.");
    ImGui::Separator();

    auto* inv = AC2::GetInventory();

    // Money Special Handling (int32_t)
    if (inv && inv->m_pList && inv->m_pList->m_pMoney)
    {
        ImGui::InputInt("Money to Add", &m_MoneyToAdd);
        if (ImGui::Button("Add Money"))
        {
            inv->m_pList->m_pMoney->m_Count += m_MoneyToAdd;
        }
    }

    ImGui::Separator();

    if (inv && inv->m_pList)
    {
        auto drawItem = [](const char* label, AC2::InventoryItem* item) {
            if (item) {
                int val = item->m_Count;
                if (ImGui::SliderInt(label, &val, 0, 99)) { // Increased max to 99 as per request
                    item->m_Count = val;
                }
            }
        };

        drawItem("Medicine", inv->m_pList->m_pMedicine);
        drawItem("Smoke Bombs", inv->m_pList->m_pSmokeBombs);
        drawItem("Bullets", inv->m_pList->m_pBullets);
        drawItem("Poison", inv->m_pList->m_pPoison);

        // Knives Special Handling (uint8_t)
        if (inv->m_pList->m_pKnives)
        {
            int val = (int)inv->m_pList->m_pKnives->m_CountByte;
            int maxVal = (int)inv->m_pList->m_pKnives->m_MaxByte;
            if (ImGui::SliderInt("Knives", &val, 0, 30)) // Knives max is usually lower, keeping reasonable cap
            {
                inv->m_pList->m_pKnives->m_CountByte = (uint8_t)val;
            }
        }
    }
    else
    {
        ImGui::TextDisabled("Inventory not available (load a save).");
    }
}

void InventoryCheats::Update()
{
    // Skip updates while in loading screen to prevent crash
    if (AC2::IsInWhiteRoom()) return;

    if (g_config.InfiniteItems)
    {
        auto* inv = AC2::GetInventory();
        if (inv && inv->m_pList)
        {
            auto refill = [](AC2::InventoryItem* item) {
                if (item) item->m_Count = item->m_MaxCount > 0 ? item->m_MaxCount : 15;
            };

            refill(inv->m_pList->m_pMedicine);
            refill(inv->m_pList->m_pSmokeBombs);
            refill(inv->m_pList->m_pBullets);
            refill(inv->m_pList->m_pPoison);

            // Knives are handled by Hook_LockKnives to prevent decrement.
            // We just ensure they are full here.
            if (inv->m_pList->m_pKnives)
            {
                uint8_t maxK = inv->m_pList->m_pKnives->m_MaxByte > 0 ? inv->m_pList->m_pKnives->m_MaxByte : 20;
                inv->m_pList->m_pKnives->m_CountByte = maxK;
            }
        }
    }
}