#include "Cheats/CharacterCheats.h"
#include "Game/Singletons.h"
#include "Game/Managers/ProgressionManager.h"
#include "imgui.h"

CharacterCheats::CharacterCheats()
{
    // Known IDs from Cheat Engine table
    m_KnownNames = {
        { AC2::ItemID::DefaultCape,  "Default (Cape)" },
        { AC2::ItemID::YoungEzio,    "Florentine Noble (Young Ezio)" },
        { AC2::ItemID::AltairArmor,  "Altair Armor" },
        { AC2::ItemID::BorgiaGuard,  "Borgia Guard" },
        { AC2::ItemID::AltairRobes,  "Altair Robes" },
        { AC2::ItemID::AltairRobes2, "Altair Robes (Disable Certain Gear)" },
        { AC2::ItemID::Desmond,      "Desmond (Disable Certain Gear)" }
    };
}

void CharacterCheats::RefreshAvailableCharacters()
{
    m_AvailableCharacters.clear();
    auto* prog = AC2::GetProgressionManager();

    // Check for pointer validity first
    if (!prog || !prog->m_pPlayerAttributes) return;

    for (int16_t i = 0; i < prog->m_AttributeCount; i++)
    {
        auto* attr = prog->m_pPlayerAttributes[i];
        // Check IsBadReadPtr on the attribute pointer itself
        if (attr && attr->m_pPlayerProfile)
        {
            AC2::ItemID id = attr->m_pPlayerProfile->m_PlayerID;
            // We store the ID for display name lookup, but we rely on the POINTER for selection
            m_AvailableCharacters.push_back({ GetCharacterName(id), id, attr->m_pPlayerProfile });
        }
    }
}

std::string CharacterCheats::GetCharacterName(AC2::ItemID id)
{
    if (m_KnownNames.count(id)) return m_KnownNames[id];
    
    char buf[32];
    snprintf(buf, sizeof(buf), "Unknown (0x%X)", (uint32_t)id);
    return std::string(buf);
}

void CharacterCheats::DrawUI()
{
    ImGui::TextWrapped("Select a character model below. You must Fast Travel to apply changes.");
    ImGui::Separator();

    if (ImGui::Button("Refresh List"))
    {
        RefreshAvailableCharacters();
    }

    auto* prog = AC2::GetProgressionManager();
    if (prog)
    {
        // Current Selection (Pointer comparison)
        AC2::PlayerProfile* currentProfilePtr = prog->m_pSelectedProfile;
        
        // Try to resolve current name
        std::string currName = "Unknown/Custom";
        if (currentProfilePtr) currName = GetCharacterName(currentProfilePtr->m_PlayerID);

        ImGui::Text("Active Profile: %s", currName.c_str());

        if (ImGui::BeginListBox("Available Models", ImVec2(-FLT_MIN, 200)))
        {
            for (const auto& charInfo : m_AvailableCharacters)
            {
                bool isSelected = (currentProfilePtr == charInfo.ptr);
                if (ImGui::Selectable(charInfo.name.c_str(), isSelected))
                {
                    prog->m_pSelectedProfile = charInfo.ptr;
                }
                if (isSelected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndListBox();
        }
    }
    else
    {
        ImGui::TextDisabled("Progression Manager not available.");
    }
}