#pragma once
#include "Cheats/CheatBase.h"

class InventoryCheats : public CheatBase
{
public:
    void DrawUI() override;
    void Update() override;
    std::string GetName() const override { return "Inventory"; }

private:
    int m_MoneyToAdd = 1000;
};
