#include "../include/Serialization/Utils/FileSystem.h"
#include <fstream>
#include <sstream>

namespace Serialization::Utils {

    json::JSON LoadJSONFromFile(const std::filesystem::path& path)
    {
        std::ifstream ifs(path);
        if (!ifs.is_open())
        {
            return json::JSON(); // Return null/empty JSON
        }
        std::stringstream ss;
        ss << ifs.rdbuf();
        return json::JSON::Load(ss.str());
    }

    bool SaveJSONToFile(const json::JSON& obj, const std::filesystem::path& path)
    {
        try
        {
            if (path.has_parent_path())
            {
                std::filesystem::create_directories(path.parent_path());
            }
            std::ofstream ofs(path);
            if (!ofs.is_open()) return false;
            
            ofs << obj.dump();
            return true;
        }
        catch (const std::filesystem::filesystem_error&)
        {
            return false;
        }
    }

}