#include "Cheats/TeleportCheats.h"
#include "Game/Singletons.h"
#include "Game/Managers/MapManager.h"
#include "Game/Entity.h"
#include "Game/Camera.h"
#include "Core/Constants.h"
#include "Trainer.h"
#include "imgui.h"
#include <windows.h>
#include <cmath>

extern Trainer::Configuration g_config;

void TeleportCheats::DrawUI()
{
    // Manual Coords (CE "X-axis/Y-axis/Z-axis" feature)
    ImGui::TextDisabled("-- Manual Coordinates --");
    auto* player = AC2::GetPlayer();
    if (player)
    {
        float coords[3] = { player->Position.x, player->Position.y, player->Position.z };
        if (ImGui::InputFloat3("X / Y / Z", coords, "%.2f"))
        {
            player->Position.x = coords[0];
            player->Position.y = coords[1];
            player->Position.z = coords[2];
        }
    }

    ImGui::Separator();

    // Map Waypoint
    ImGui::TextDisabled("-- Map Waypoint --");
    
    auto* waypoint = (AC2::MapWaypoint*)AC2::GetMapManager();
    // MapManager is actually the CurrentWaypoint struct in this game's hook context
    // mapMgr IS the waypoint object based on CE logic (pMapWayp)
    bool hasWaypoint = (waypoint && (waypoint->Position.x != 0 || waypoint->Position.y != 0));

    if (ImGui::Button("Teleport to Waypoint"))
    {
        TeleportToWaypoint();
    }
    
    if (hasWaypoint)
    {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "[Set: %.1f, %.1f]", waypoint->Position.x, waypoint->Position.y);
    }
    else
    {
        ImGui::SameLine();
        ImGui::TextDisabled("(No Marker)");
    }
    
    ImGui::Separator();

    // Free Roam (Fly Mode)
    ImGui::TextDisabled("-- Free Roam --");
    ImGui::Checkbox("Enable Free Roam / Fly Mode", &g_config.FlyMode.get());
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Use Numpad 8/2/4/6/7/9 to move. 'Ignore Fall Damage' is recommended.");

    ImGui::SliderFloat("Fly Speed", &g_config.FlySpeed.get(), 1.0f, 50.0f, "%.1f");
    
    ImGui::Separator();

    // Save / Restore
    ImGui::TextDisabled("-- Save & Restore --");
    if (player)
    {
        if (ImGui::Button("Save Position [F11]"))
        {
            m_SavedPos = player->Position;
            m_HasSavedPos = true;
        }
        
        ImGui::SameLine();
        ImGui::BeginDisabled(!m_HasSavedPos);
        if (ImGui::Button("Restore Position [F12]"))
        {
            TeleportTo(m_SavedPos);
        }
        ImGui::EndDisabled();
    }
}

void TeleportCheats::Update()
{
    // Skip updates while in loading screen to prevent crash
    if (AC2::IsInWhiteRoom()) return;

    UpdateFlyMode();
    HandleSaveRestore();

    // Shortcut for Waypoint
    if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) && (GetAsyncKeyState('T') & 0x8000) && (GetAsyncKeyState('T') & 1))
    {
        TeleportToWaypoint();
    }
}

void TeleportCheats::TeleportTo(const AC2::Scimitar::Vector3& pos)
{
    auto* player = AC2::GetPlayer();
    if (player)
    {
        player->Position = pos;
    }
}

void TeleportCheats::TeleportToWaypoint()
{
    auto* waypoint = (AC2::MapWaypoint*)AC2::GetMapManager(); 
    auto* player = AC2::GetPlayer();
    
    if (player && waypoint)
    {
        AC2::Scimitar::Vector3 target = waypoint->Position;
        
        // Validate coordinates - no waypoint if both zero
        if (target.x == 0 && target.y == 0)
            return;

        // Apply height adjustment for safe landing
        if (target.z != 0.0f)
            target.z += AC2::Constants::WAYPOINT_Z_OFFSET;
        if (target.z < AC2::Constants::DROP_HEIGHT_DEFAULT) 
            target.z = AC2::Constants::DROP_HEIGHT_DEFAULT;

        TeleportTo(target);
        
        // Enable fall damage protection if god mode is off
        if (!g_config.GodMode) 
            g_config.IgnoreFallDamage = true; 
    }
}

void TeleportCheats::UpdateFlyMode()
{
    if (!g_config.FlyMode) return;

    auto* player = AC2::GetPlayer();
    if (!player) return;

    // Enforce fall damage protection while flying
    g_config.IgnoreFallDamage = true;

    // Get Rotation Yaw from player. 
    // Entity->Rotation is X,Y,Z,W (quaternion). 
    // Convert quaternion to forward vector for movement
    // Quaternion to forward: forward = (1 - 2*(y² + z²), 2*(x*y + w*z), 2*(x*z - w*y))
    float qx = player->Rotation.x;
    float qy = player->Rotation.y;
    float qz = player->Rotation.z;
    float qw = player->Rotation.w;
    
    // Calculate forward vector (X, Y plane movement)
    // Quaternion forward: rotate (0, 1, 0) by quaternion
    float fwdX = 1.0f - 2.0f * (qy * qy + qz * qz);
    float fwdY = 2.0f * (qx * qy + qw * qz);
    
    // Calculate right vector for strafing (perpendicular to forward in 2D)
    // In 2D, right = (-forward.y, forward.x)
    float rightX = -fwdY;
    float rightY = fwdX;
    
    float speed = g_config.FlySpeed * 0.1f; // Scale down for per-frame
    
    // Numpad 8 (Forward)
    if (GetAsyncKeyState(VK_NUMPAD8) & 0x8000)
    {
        player->Position.x += fwdX * speed;
        player->Position.y += fwdY * speed;
    }
    // Numpad 2 (Backward)
    if (GetAsyncKeyState(VK_NUMPAD2) & 0x8000)
    {
        player->Position.x -= fwdX * speed;
        player->Position.y -= fwdY * speed;
    }
    // Numpad 4 (Left / Strafe)
    if (GetAsyncKeyState(VK_NUMPAD4) & 0x8000)
    {
        player->Position.x -= rightX * speed;
        player->Position.y -= rightY * speed;
    }
    // Numpad 6 (Right / Strafe)
    if (GetAsyncKeyState(VK_NUMPAD6) & 0x8000)
    {
        player->Position.x += rightX * speed;
        player->Position.y += rightY * speed;
    }
    // Numpad 9 (Up)
    if (GetAsyncKeyState(VK_NUMPAD9) & 0x8000)
    {
        player->Position.z += speed;
    }
    // Numpad 7 (Down)
    if (GetAsyncKeyState(VK_NUMPAD7) & 0x8000)
    {
        player->Position.z -= speed;
    }
}

void TeleportCheats::HandleSaveRestore()
{
    auto* player = AC2::GetPlayer();
    if (!player) return;

    // F11 Save
    if (GetAsyncKeyState(VK_F11) & 1)
    {
        m_SavedPos = player->Position;
        m_HasSavedPos = true;
    }
    // F12 Restore
    if ((GetAsyncKeyState(VK_F12) & 1) && m_HasSavedPos)
    {
        TeleportTo(m_SavedPos);
    }
}
