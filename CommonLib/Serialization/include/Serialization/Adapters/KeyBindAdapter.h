#pragma once
#include "../Serialization.h"
#include "KeyBind.h"

namespace Serialization {

    // Adapter to serialize KeyBind as [Key, Ctrl, Shift, Alt]
    struct KeyBindAdapter : public JSONAdapter<KeyBind, JSON::Class::Array>
    {
        using BaseAdapter::BaseAdapter;
        
        JSON ToJSON()
        {
            JSON arr = JSON::Make(JSON::Class::Array);
            arr.append((long long)source.Key);
            arr.append(source.Ctrl);
            arr.append(source.Shift);
            arr.append(source.Alt);
            return arr;
        }

        bool FromJSON(const JSON& obj)
        {
            if (!IsCorrectJSONType(obj)) return false;
            auto range = obj.ArrayRange();
            auto it = range.begin();
            
            if (it != range.end()) { source.Key = (unsigned int)(*it).ToInt(); ++it; }
            if (it != range.end()) { source.Ctrl = (*it).ToBool(); ++it; }
            if (it != range.end()) { source.Shift = (*it).ToBool(); ++it; }
            if (it != range.end()) { source.Alt = (*it).ToBool(); ++it; }
            
            return true;
        }
    };
}