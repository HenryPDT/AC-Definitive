#pragma once
#include "../Serialization.h"
#include <string>
#include <sstream>
#include <iomanip>

namespace Serialization {

    // Serializes uint64_t as a Hex String (e.g. "0xFF")
    struct HexStringAdapter : public JSONAdapter<uint64_t, JSON::Class::String>
    {
        using BaseAdapter::BaseAdapter;

        JSON ToJSON()
        {
            std::stringstream ss;
            ss << "0x" << std::uppercase << std::hex << source;
            return ss.str();
        }

        bool FromJSON(const JSON& obj)
        {
            if (!IsCorrectJSONType(obj)) return false;
            std::string s = obj.ToString();
            try {
                source = std::stoull(s, nullptr, 16);
                return true;
            }
            catch (...) {
                return false;
            }
        }
    };
}