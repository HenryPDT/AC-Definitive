#pragma once

#include <filesystem>
#include "../SimpleJSON/json.hpp"

namespace Serialization::Utils {
    
    json::JSON LoadJSONFromFile(const std::filesystem::path& path);
    bool SaveJSONToFile(const json::JSON& obj, const std::filesystem::path& path);

}