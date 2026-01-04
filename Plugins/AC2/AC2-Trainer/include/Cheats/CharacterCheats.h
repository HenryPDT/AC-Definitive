#pragma once
#include "Cheats/CheatBase.h"
#include <map>
#include <vector>
#include <string>
#include "Game/Enums/ItemIDs.h"
#include "Game/Managers/ProgressionManager.h"

class CharacterCheats : public CheatBase
{
public:
    CharacterCheats();
    void DrawUI() override;
    std::string GetName() const override { return "Character Switcher"; }

private:
    void RefreshAvailableCharacters();
    std::string GetCharacterName(AC2::ItemID id);

    struct CharInfo {
        std::string name;
        AC2::ItemID id;
        AC2::PlayerProfile* ptr;
    };
    std::vector<CharInfo> m_AvailableCharacters;
    std::map<AC2::ItemID, std::string> m_KnownNames;
};