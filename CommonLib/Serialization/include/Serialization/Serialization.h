#pragma once

#include "SimpleJSON/json.hpp"
#include <filesystem>

namespace Serialization
{
    using JSON = json::JSON;

    namespace Internal
    {
        template<typename Adapter> 
        bool TryToReadVariableFromJSONObjectUsingAdapter(JSON& jsonObject, const std::string& key, Adapter&& adaptedVariable)
        {
            if (JSON* foundKey = jsonObject.FindByKey(key))
            {
                return adaptedVariable.FromJSON(*foundKey);
            }
            return false;
        }

        template<typename Adapter> 
        void WriteVariableAsJSONObjectMemberUsingAdapter(JSON& jsonObject, const std::string& key, Adapter&& adaptedVariable)
        {
            jsonObject[key] = adaptedVariable.ToJSON();
        }
    }
}

// Global namespace shortcuts for serialization definitions
using Serialization::JSON;

template<class AdaptedCls, JSON::Class jsonType>
struct JSONAdapter
{
	using BaseAdapter = JSONAdapter<AdaptedCls, jsonType>;
	AdaptedCls& source;
	JSONAdapter(AdaptedCls& source) : source(source) {}
	
    static inline json::JSON::Class GetAdapterType()
	{
		return jsonType;
	}
	
    bool IsCorrectJSONType(const JSON& obj)
	{
		return obj.JSONType() == jsonType;
	}
};