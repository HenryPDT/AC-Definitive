#pragma once
#include "Cheats/CheatBase.h"
#include "Scimitar/math.h"

class TeleportCheats : public CheatBase
{
public:
    void DrawUI() override;
    void Update() override;
    std::string GetName() const override { return "Teleport"; }

private:
    void TeleportTo(const AC2::Scimitar::Vector3& pos);
    void TeleportToWaypoint();
    
    // Free Roam
    void UpdateFlyMode();
    void UpdateCameraFlyMode();
    
    // Save/Restore
    void HandleSaveRestore();

    AC2::Scimitar::Vector3 m_SavedPos = { 0, 0, 0 };
    bool m_HasSavedPos = false;

    // Edge detection for keybinds
    bool m_WaypointKeyWasDown = false;
    bool m_SaveKeyWasDown = false;
    bool m_RestoreKeyWasDown = false;
};
