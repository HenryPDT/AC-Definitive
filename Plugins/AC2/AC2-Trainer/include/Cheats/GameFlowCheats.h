#pragma once
#include "Cheats/CheatBase.h"

class GameFlowCheats : public CheatBase
{
public:
    void DrawUI() override;
    void Update() override;
    std::string GetName() const override { return "Game Flow"; }

private:
    const char* GetBinkFileName();
    void TriggerSkip();

    bool m_BinkKeyWasDown = false;
};
