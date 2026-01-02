#pragma once

#include "Serialization.h"
#include "Adapters/BooleanAdapter.h"
#include "Adapters/NumericAdapters.h"
#include "Adapters/StringAdapter.h"
#include "Adapters/EnumAdapter.h"
#include "Adapters/KeyBindAdapter.h"
#include <vector>
#include <string>

namespace Serialization {

class ConfigSection;

// Non-templated base so that the templated derived classes can be stored in a vector.
class ConfigEntryBase
{
protected:
    friend ConfigSection;
    const std::string m_Name;
    ConfigEntryBase(const std::string& name);
public:
    virtual bool FromJSON(JSON& cfg) = 0;
    virtual void ToJSON(JSON& cfg) = 0;
private:
    void RegisterWithContext();
};

template<class UnderlyingType, class Adapter>
class ConfigProperty : public ConfigEntryBase
{
private:
    UnderlyingType m_Value;
public:
    UnderlyingType& get() { return m_Value; }
    UnderlyingType* operator->() { return &m_Value; }
    operator UnderlyingType& () { return m_Value; }
    void operator=(const UnderlyingType& rhs) { m_Value = rhs; }
    
    ConfigProperty(const std::string& name)
        : ConfigEntryBase(name)
    {}
    
    ConfigProperty(const std::string& name, const UnderlyingType& initialValue)
        : ConfigEntryBase(name)
        , m_Value(initialValue)
    {}
    
    virtual bool FromJSON(JSON& jsonValue) override
    {
        return Adapter(m_Value).FromJSON(jsonValue);
    }
    
    virtual void ToJSON(JSON& jsonValue) override
    {
        jsonValue = Adapter(m_Value).ToJSON();
    }
};

class ConfigSection
{
private:
    std::vector<ConfigEntryBase*> m_Properties;
    ConfigSection* m_PreviousContext;
    
protected:
    // Helper type for constructor overloading
    struct InternalCtorTag {};
    
    ConfigSection(InternalCtorTag);
    void RestoreContext();

public:
    void SectionToJSON(JSON& jsonOut);
    bool SectionFromJSON(JSON& jsonObject);

private:
    friend ConfigEntryBase;
    void AddProperty(ConfigEntryBase& prop);
};

// Adapter to allow nesting sections inside other sections
class ConfigSectionAdapter : public JSONAdapter<ConfigSection, JSON::Class::Object>
{
public:
    using BaseAdapter::BaseAdapter;
    
    JSON ToJSON()
    {
        JSON result;
        this->source.SectionToJSON(result);
        return result;
    }
    
    bool FromJSON(JSON& obj)
    {
        if (!this->IsCorrectJSONType(obj))
        {
            return false;
        }
        this->source.SectionFromJSON(obj);
        return true;
    }
};

// Macros for ease of use
#define SECTION_CTOR(SubclsName) \
    SubclsName() : ConfigSection(InternalCtorTag()) { RestoreContext(); }

#define PROPERTY(varName, VarType, AdapterType, optionalDefaultValue) \
    Serialization::ConfigProperty<VarType, AdapterType> varName{ #varName, optionalDefaultValue };

}