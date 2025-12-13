#pragma once

#include "../Serialization.h"
#include <string>

namespace Serialization {

struct StringAdapter : public JSONAdapter<std::string, JSON::Class::String>
{
	using BaseAdapter::BaseAdapter;
	
    // Serialize.
	JSON ToJSON()
	{
		return source;
	}
	
    // Deserialize.
	bool FromJSON(const JSON& obj)
	{
		if (!this->IsCorrectJSONType(obj))
		{
			return false;
		}
		source = obj.ToString();
		return true;
	}
};

struct StringAdapterNoEscape : public JSONAdapter<std::string, JSON::Class::String>
{
	using BaseAdapter::BaseAdapter;
	
    // Serialize.
	JSON ToJSON()
	{
		return source;
	}
	
    // Deserialize.
	bool FromJSON(const JSON& obj)
	{
		if (!this->IsCorrectJSONType(obj))
		{
			return false;
		}
		source = obj.ToStringNoEscape();
		return true;
	}
};

}