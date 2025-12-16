#pragma once

#include "imgui.h"
#include "Serialization/EnumFactory.h"
#include <algorithm>
#include <vector>
#include "KeyBind.h"

namespace ImGui {

bool KeyBindInput(const char* label, KeyBind& keybind);

template<typename EnumType>
bool DrawEnumPicker(const char* label, EnumType& currentValueInOut, ImGuiComboFlags flags = 0)
{
    bool isNewSelection = false;
    auto itemsStrings = enum_reflection<EnumType>::GetAllStrings();
    auto itemsValues = enum_reflection<EnumType>::GetAllValues();
    auto it = std::find(itemsValues.begin(), itemsValues.end(), currentValueInOut);
    
    int item_current_idx = 0;
    if (it != itemsValues.end()) {
        item_current_idx = (int)(it - itemsValues.begin());
    }

    // Safety check in case the index is out of bounds (shouldn't happen if initialized correctly)
    if (item_current_idx < 0 || static_cast<size_t>(item_current_idx) >= itemsStrings.size()) {
        item_current_idx = 0;
    }

    if (ImGui::BeginCombo(label, itemsStrings[item_current_idx], flags))
    {
        for (int n = 0; static_cast<size_t>(n) < itemsStrings.size(); n++)
        {
            const bool is_selected = (item_current_idx == n);
            if (ImGui::Selectable(itemsStrings[n], is_selected)) {
                item_current_idx = n;
                currentValueInOut = itemsValues[n];
                isNewSelection = true;
            }
        }
        ImGui::EndCombo();
    }
    return isNewSelection;
}
} // namespace ImGui
