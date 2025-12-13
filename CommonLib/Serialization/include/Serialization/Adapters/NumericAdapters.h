#pragma once

#include "../Serialization.h"
#include <utility>

namespace Serialization {

inline std::pair<bool, double> JSONToFloat(const JSON& obj)
{
	if (obj.JSONType() == JSON::Class::Floating)
	{
		return { true, obj.ToFloat() };
	}
	if (obj.JSONType() == JSON::Class::Integral)
	{
		return { true, (double)obj.ToInt() };
	}
	return { false, 0 };
}

template<class CastTo>
struct IntegerAdapter_template : public JSONAdapter<CastTo, JSON::Class::Integral>
{
	using JSONAdapter<CastTo, JSON::Class::Integral>::JSONAdapter;
	
    // Serialize.
	JSON ToJSON()
	{
		return (long long)this->source;
	}
	
    // Deserialize.
	bool FromJSON(const JSON& obj)
	{
		if (!this->IsCorrectJSONType(obj))
		{
			return false;
		}
		this->source = (CastTo)obj.ToInt();
		return true;
	}
};

template<class CastTo>
inline IntegerAdapter_template<CastTo> IntegerAdapter(CastTo& source)
{
	return { source };
}

template<class CastTo> 
struct NumericAdapter_template : public JSONAdapter<CastTo, JSON::Class::Floating>
{
	using JSONAdapter<CastTo, JSON::Class::Floating>::JSONAdapter;
	
    bool FromJSON(const JSON& obj)
	{
		auto isNumeric__asFloat = JSONToFloat(obj);
		if (isNumeric__asFloat.first)
		{
			this->source = (CastTo)isNumeric__asFloat.second;
			return true;
		}
		return false;
	}
	
    JSON ToJSON()
	{
		return (double)this->source;
	}
};

template<class CastTo> 
NumericAdapter_template<CastTo> NumericAdapter(CastTo& source)
{
	return NumericAdapter_template<CastTo>(source);
}

// NOTE: Vector adapters (Vector2f, etc.) omitted to remove dependency on external math library.
// If needed, they can be re-added here later using standard structs or ImVec types.

}