#pragma once
#include "Cheats/CheatBase.h"

class WorldCheats : public CheatBase
{
public:
    void DrawUI() override;
    void Update() override;
    std::string GetName() const override { return "World"; }
};