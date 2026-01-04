#pragma once
#include "Cheats/CheatBase.h"

class PlayerCheats : public CheatBase
{
public:
    void DrawUI() override; // Draws Player Status section
    void DrawMiscUI();      // Draws Speed/Resize for Misc tab
    void Update() override;
    std::string GetName() const override { return "Player Status"; }

private:
    void DrawPlayerStatus();

    void UpdateGodMode();
    void UpdateInvisibility();
    void UpdateNotoriety();
    void UpdateSpeed();
    void UpdateScale();
    void UpdateMiscAndFallDamage();
};
