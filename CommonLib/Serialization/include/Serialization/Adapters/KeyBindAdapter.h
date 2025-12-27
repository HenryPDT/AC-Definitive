#pragma once
#include "../Serialization.h"
#include "KeyBind.h"

namespace Serialization {

    // Adapter to serialize KeyBind as [KeyboardKey, Ctrl, Shift, Alt, ControllerKey, Version=1]
    struct KeyBindAdapter : public JSONAdapter<KeyBind, JSON::Class::Array>
    {
        using BaseAdapter::BaseAdapter;
        
        JSON ToJSON()
        {
            JSON arr = JSON::Make(JSON::Class::Array);
            arr.append((long long)source.KeyboardKey);
            arr.append(source.Ctrl);
            arr.append(source.Shift);
            arr.append(source.Alt);
            arr.append((long long)source.ControllerKey);
            arr.append(1); // Version 1 signature
            return arr;
        }

        bool FromJSON(const JSON& obj)
        {
            if (!IsCorrectJSONType(obj)) return false;
            
            const int count = obj.size();
            auto range = obj.ArrayRange();
            auto it = range.begin();

            // Migration Logic
            if (count == 5)
            {
                // Old Format: [Key, C, S, A, DeviceEnum]
                unsigned int rawKey = 0;
                bool c = false, s = false, a = false;
                int device = 0;

                if (it != range.end()) { rawKey = static_cast<unsigned int>((*it).ToInt()); ++it; }
                if (it != range.end()) { c = (*it).ToBool(); ++it; }
                if (it != range.end()) { s = (*it).ToBool(); ++it; }
                if (it != range.end()) { a = (*it).ToBool(); ++it; }
                if (it != range.end()) { device = static_cast<int>((*it).ToInt()); ++it; }

                source.Ctrl = c; source.Shift = s; source.Alt = a;
                if (device == 1) // Controller
                {
                    source.KeyboardKey = 0;
                    source.ControllerKey = rawKey;
                }
                else // Keyboard
                {
                    source.KeyboardKey = rawKey;
                    source.ControllerKey = 0;
                }
                return true;
            }

            // New Format: [KB, C, S, A, Pad, Ver]
            if (it != range.end()) { source.KeyboardKey = static_cast<unsigned int>((*it).ToInt()); ++it; }
            if (it != range.end()) { source.Ctrl = (*it).ToBool(); ++it; }
            if (it != range.end()) { source.Shift = (*it).ToBool(); ++it; }
            if (it != range.end()) { source.Alt = (*it).ToBool(); ++it; }
            if (it != range.end()) { source.ControllerKey = static_cast<unsigned int>((*it).ToInt()); ++it; }
            // Ignore version field
            
            return true;
        }
    };
}