#include "Cheats/TeleportCheats.h"
#include "Game/Singletons.h"
#include "Game/Managers/MapManager.h"
#include "Game/Entity.h"
#include "Game/Camera.h"
#include "Core/Constants.h"
#include "Trainer.h"
#include "Hooks.h"
#include "imgui.h"
#include <ImGuiConfigUtils.h>
#include <cmath>

extern Trainer::Configuration g_config;

void TeleportCheats::DrawUI()
{
    if (AC2::IsInWhiteRoom())
    {
        ImGui::TextDisabled("Unavailable while loading/in white room");
        return;
    }

    auto* player = AC2::GetPlayer();

    // Manual Coordinates Section
    if (ImGui::CollapsingHeader("Position", ImGuiTreeNodeFlags_DefaultOpen))
    {
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
        else
        {
            ImGui::TextDisabled("Player not available.");
        }

        // Save & Restore inline
        if (player)
        {
            ImGui::Spacing();
            std::string saveLabel = "Save [" + g_config.Key_SavePosition.get().ToString() + "]";
            if (ImGui::Button(saveLabel.c_str()))
            {
                m_SavedPos = player->Position;
                m_HasSavedPos = true;
            }
            
            ImGui::SameLine();
            ImGui::BeginDisabled(!m_HasSavedPos);
            std::string restoreLabel = "Restore [" + g_config.Key_RestorePosition.get().ToString() + "]";
            if (ImGui::Button(restoreLabel.c_str()))
            {
                TeleportTo(m_SavedPos);
            }
            ImGui::EndDisabled();

            if (m_HasSavedPos)
            {
                ImGui::SameLine();
                ImGui::TextDisabled("(%.0f, %.0f, %.0f)", m_SavedPos.x, m_SavedPos.y, m_SavedPos.z);
            }
        }
    }

    // Waypoint Section
    if (ImGui::CollapsingHeader("Map Waypoint", ImGuiTreeNodeFlags_DefaultOpen))
    {
        auto* waypoint = (AC2::MapWaypoint*)AC2::GetMapManager();
        bool hasWaypoint = (waypoint && (waypoint->Position.x != 0 || waypoint->Position.y != 0));

        std::string tpLabel = "Teleport [" + g_config.Key_TeleportWaypoint.get().ToString() + "]";
        if (ImGui::Button(tpLabel.c_str()))
        {
            TeleportToWaypoint();
        }
        
        if (hasWaypoint)
        {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "Marker: %.0f, %.0f", waypoint->Position.x, waypoint->Position.y);
        }
        else
        {
            ImGui::SameLine();
            ImGui::TextDisabled("No marker set");
        }
    }

    // Free Roam Section
    if (ImGui::CollapsingHeader("Free Roam", ImGuiTreeNodeFlags_DefaultOpen))
    {
        static const char* freeRoamModes[] = { "Off", "Player", "Camera" };
        if (ImGui::Combo("Mode", &g_config.FreeRoamTarget.get(), freeRoamModes, 3))
        {
            // Reset camera position when enabling camera mode
            if (g_config.FreeRoamTarget == 2)
            {
                float* camPos = Hooks::GetCameraPosPointer();
                if (camPos)
                {
                    void* pFreeCameraObj = Hooks::GetFreeCameraObjectPointer();
                    if (pFreeCameraObj)
                    {
                        float* pRealCamPos = reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(pFreeCameraObj) + 0x30);
                        camPos[0] = pRealCamPos[0];
                        camPos[1] = pRealCamPos[1];
                        camPos[2] = pRealCamPos[2];
                    }
                    else if (player)
                    {
                        camPos[0] = player->Position.x;
                        camPos[1] = player->Position.y;
                        camPos[2] = player->Position.z;
                    }
                }
            }
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Off = Disabled\nPlayer = Fly (camera follows)\nCamera = Move camera only");

        ImGui::DragFloat("Fly Speed", &g_config.FlySpeed.get(), 0.1f, 0.5f, 20.0f, "%.1f");
        ImGui::SameLine();
        if (ImGui::Button("Reset##FlySpeed")) g_config.FlySpeed = 1.0f;
    }

    // Camera Section
    if (ImGui::CollapsingHeader("Camera"))
    {
        void* pFreeCameraObj = Hooks::GetFreeCameraObjectPointer();
        void* pFreeCam = Hooks::GetFreeCamPointer();
        
        if (pFreeCameraObj && pFreeCam)
        {
            float* pCamPos = reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(pFreeCameraObj) + 0x30);
            float* pCamData = reinterpret_cast<float*>(pFreeCam);
            
            float yawComponent1 = pCamData[0x20 / 4];
            float yawComponent2 = pCamData[0x30 / 4];
            float pitchValue = pCamData[0x38 / 4];
            
            float yaw = std::atan2(-yawComponent1, yawComponent2) * 57.29578f;
            float pitch = std::asin(pitchValue) * 57.29578f;

            ImGui::Text("Pos: %.1f, %.1f, %.1f", pCamPos[0], pCamPos[1], pCamPos[2]);
            ImGui::Text("Rot: Yaw %.0f°, Pitch %.0f°", yaw, pitch);
        }
        else
        {
            ImGui::TextDisabled("Camera data unavailable.");
        }

        ImGui::Spacing();
        ImGui::DragFloat("FOV", &g_config.CameraFOV.get(), 0.01f, 0.1f, 3.0f, "%.2f");
        if (ImGui::IsItemEdited())
        {
            void* pFreeRoam = Hooks::GetFreeRoamPointer();
            if (pFreeRoam)
            {
                float* pFOV = reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(pFreeRoam) + 0x30);
                *pFOV = g_config.CameraFOV;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset##FOV"))
        {
            g_config.CameraFOV = 0.81f;
            void* pFreeRoam = Hooks::GetFreeRoamPointer();
            if (pFreeRoam)
            {
                float* pFOV = reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(pFreeRoam) + 0x30);
                *pFOV = 0.81f;
            }
        }
    }
}

void TeleportCheats::Update()
{
    // Skip updates while in loading screen to prevent crash
    if (AC2::IsInWhiteRoom()) return;

    UpdateFlyMode();
    UpdateCameraFlyMode();
    HandleSaveRestore();

    // Shortcut for Waypoint (edge detection)
    bool waypointKeyDown = g_config.Key_TeleportWaypoint.get().IsPressed();
    if (waypointKeyDown && !m_WaypointKeyWasDown)
    {
        TeleportToWaypoint();
    }
    m_WaypointKeyWasDown = waypointKeyDown;
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
    // Skip if not in Player mode (1)
    if (g_config.FreeRoamTarget != 1) return;

    auto* player = AC2::GetPlayer();
    if (!player) return;

    // Enforce fall damage protection while flying
    g_config.IgnoreFallDamage = true;

    // Get camera yaw/pitch from pFreeCam
    void* pFreeCam = Hooks::GetFreeCamPointer();
    if (!pFreeCam) return;

    float* pCamData = reinterpret_cast<float*>(pFreeCam);
    float yawComponent1 = pCamData[0x20 / 4]; // [pFreeCam+0x20]
    float yawComponent2 = pCamData[0x30 / 4]; // [pFreeCam+0x30]
    float pitchValue = pCamData[0x38 / 4];    // [pFreeCam+0x38]
    
    // CE: yaw = atan2(-[addrYaw+0x20], [addrYaw+0x30])
    float yaw = std::atan2(-yawComponent1, yawComponent2);
    // CE: pitch = asin(fPitch), then z = cos(90-pitch) * speed
    float pitch = std::asin(pitchValue);
    float zStep = std::cos(1.5708f - pitch); // cos(PI/2 - pitch) = sin(pitch)
    
    // Forward/backward vectors based on camera yaw
    float cosYaw = std::cos(yaw);
    float sinYaw = std::sin(yaw);
    
    float speed = g_config.FlySpeed * 0.1f;
    
    // Forward (Numpad 8): x += cos(yaw), y -= sin(yaw), z += zStep (CE logic)
    if (g_config.Key_FlyForward.get().IsPressed())
    {
        player->Position.x += cosYaw * speed;
        player->Position.y -= sinYaw * speed;
        player->Position.z += zStep * speed;
    }
    // Backward (Numpad 2): opposite of forward
    if (g_config.Key_FlyBackward.get().IsPressed())
    {
        player->Position.x -= cosYaw * speed;
        player->Position.y += sinYaw * speed;
        player->Position.z -= zStep * speed;
    }
    // Left (Numpad 4): yaw - 90 degrees (no Z change)
    if (g_config.Key_FlyLeft.get().IsPressed())
    {
        float leftCos = std::cos(yaw - 1.5708f);
        float leftSin = std::sin(yaw - 1.5708f);
        player->Position.x += leftCos * speed;
        player->Position.y -= leftSin * speed;
    }
    // Right (Numpad 6): yaw + 90 degrees (no Z change)
    if (g_config.Key_FlyRight.get().IsPressed())
    {
        float rightCos = std::cos(yaw + 1.5708f);
        float rightSin = std::sin(yaw + 1.5708f);
        player->Position.x += rightCos * speed;
        player->Position.y -= rightSin * speed;
    }
    // Up (Numpad 9): pitch-based up/down
    if (g_config.Key_FlyUp.get().IsPressed())
    {
        float nInverse = (pitch >= 0) ? 1.0f : -1.0f;
        player->Position.z += zStep * speed * nInverse;
    }
    // Down (Numpad 7): pitch-based down
    if (g_config.Key_FlyDown.get().IsPressed())
    {
        float nInverse = (pitch >= 0) ? 1.0f : -1.0f;
        player->Position.z -= zStep * speed * nInverse;
    }
}

void TeleportCheats::HandleSaveRestore()
{
    auto* player = AC2::GetPlayer();
    if (!player) return;

    // Save (edge detection)
    bool saveKeyDown = g_config.Key_SavePosition.get().IsPressed();
    if (saveKeyDown && !m_SaveKeyWasDown)
    {
        m_SavedPos = player->Position;
        m_HasSavedPos = true;
    }
    m_SaveKeyWasDown = saveKeyDown;

    // Restore (edge detection)
    bool restoreKeyDown = g_config.Key_RestorePosition.get().IsPressed();
    if (restoreKeyDown && !m_RestoreKeyWasDown && m_HasSavedPos)
    {
        TeleportTo(m_SavedPos);
    }
    m_RestoreKeyWasDown = restoreKeyDown;
}

void TeleportCheats::UpdateCameraFlyMode()
{
    // Skip if not in Camera mode (2)
    if (g_config.FreeRoamTarget != 2) return;

    float* camPos = Hooks::GetCameraPosPointer();
    if (!camPos) return;

    // Get camera yaw from pFreeCam (same as player fly mode)
    void* pFreeCam = Hooks::GetFreeCamPointer();
    if (!pFreeCam) return;

    // Initialize camera position from current camera if not set
    // Initialize camera position from current camera if not set
    if (camPos[0] == 0 && camPos[1] == 0 && camPos[2] == 0)
    {
        // Try to get actual camera position first
        void* pFreeCameraObj = Hooks::GetFreeCameraObjectPointer();
        if (pFreeCameraObj)
        {
            float* pRealCamPos = reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(pFreeCameraObj) + 0x30);
            camPos[0] = pRealCamPos[0];
            camPos[1] = pRealCamPos[1];
            camPos[2] = pRealCamPos[2];
        }
        else
        {
            // Fallback to player position
            auto* player = AC2::GetPlayer();
            if (player)
            {
                camPos[0] = player->Position.x;
                camPos[1] = player->Position.y;
                camPos[2] = player->Position.z;
            }
        }
    }

    float* pCamData = reinterpret_cast<float*>(pFreeCam);
    float yawComponent1 = pCamData[0x20 / 4];
    float yawComponent2 = pCamData[0x30 / 4];
    float pitchValue = pCamData[0x38 / 4];
    float yaw = std::atan2(-yawComponent1, yawComponent2);
    float pitch = std::asin(pitchValue);
    float zStep = std::cos(1.5708f - pitch);
    
    float cosYaw = std::cos(yaw);
    float sinYaw = std::sin(yaw);
    float speed = g_config.FlySpeed * 0.1f;
    
    // Forward (Numpad 8): camera-relative with pitch Z
    if (g_config.Key_FlyForward.get().IsPressed())
    {
        camPos[0] += cosYaw * speed;
        camPos[1] -= sinYaw * speed;
        camPos[2] += zStep * speed;
    }
    // Backward (Numpad 2)
    if (g_config.Key_FlyBackward.get().IsPressed())
    {
        camPos[0] -= cosYaw * speed;
        camPos[1] += sinYaw * speed;
        camPos[2] -= zStep * speed;
    }
    // Left (Numpad 4): no Z change
    if (g_config.Key_FlyLeft.get().IsPressed())
    {
        float leftCos = std::cos(yaw - 1.5708f);
        float leftSin = std::sin(yaw - 1.5708f);
        camPos[0] += leftCos * speed;
        camPos[1] -= leftSin * speed;
    }
    // Right (Numpad 6): no Z change
    if (g_config.Key_FlyRight.get().IsPressed())
    {
        float rightCos = std::cos(yaw + 1.5708f);
        float rightSin = std::sin(yaw + 1.5708f);
        camPos[0] += rightCos * speed;
        camPos[1] -= rightSin * speed;
    }
    // Up (Numpad 9): pitch-based
    if (g_config.Key_FlyUp.get().IsPressed())
    {
        float nInverse = (pitch >= 0) ? 1.0f : -1.0f;
        camPos[2] += zStep * speed * nInverse;
    }
    // Down (Numpad 7): pitch-based
    if (g_config.Key_FlyDown.get().IsPressed())
    {
        float nInverse = (pitch >= 0) ? 1.0f : -1.0f;
        camPos[2] -= zStep * speed * nInverse;
    }
}
