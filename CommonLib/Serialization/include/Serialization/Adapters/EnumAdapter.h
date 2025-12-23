#pragma once

#include "../Serialization.h"
#include "../EnumFactory.h"
#include <optional>

namespace Serialization {

template<typename EnumType>
class EnumAdapter_template : public JSONAdapter<EnumType, JSON::Class::String>
{
public:
    using JSONAdapter<EnumType, JSON::Class::String>::JSONAdapter;

    bool FromJSON(JSON& obj)
    {
        if (!this->IsCorrectJSONType(obj))
        {
            return false;
        }
        std::optional<EnumType> parsedValue = enum_reflection<EnumType>::GetValue(obj.ToString());
        if (!parsedValue) {
            return false;
        }
        this->source = *parsedValue;
        return true;
    }

    JSON ToJSON()
    {
        return enum_reflection<EnumType>::GetString(this->source);
    }
};

template<typename EnumType>
class NumericEnumAdapter_template : public JSONAdapter<EnumType, JSON::Class::Integral>
{
public:
    using JSONAdapter<EnumType, JSON::Class::Integral>::JSONAdapter;

    bool FromJSON(JSON& obj)
    {
        if (!this->IsCorrectJSONType(obj))
        {
            return false;
        }
        long long value = obj.ToInt();
        this->source = static_cast<EnumType>(value);
        return true;
    }

    JSON ToJSON()
    {
        return static_cast<long long>(this->source);
    }
};

template<typename EnumType>
EnumAdapter_template<EnumType> EnumAdapter(EnumType& source)
{
    return { source };
}

template<typename EnumType>
NumericEnumAdapter_template<EnumType> NumericEnumAdapter(EnumType& source)
{
    return { source };
}

}