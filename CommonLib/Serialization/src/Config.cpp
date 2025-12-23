#include "../include/Serialization/Config.h"

namespace Serialization {

// Global context pointer for registration
static ConfigSection* g_CurrentContext = nullptr;

ConfigEntryBase::ConfigEntryBase(const std::string& name)
    : m_Name(name)
{
    RegisterWithContext();
}

void ConfigEntryBase::RegisterWithContext()
{
    if (g_CurrentContext)
    {
        g_CurrentContext->AddProperty(*this);
    }
}

ConfigSection::ConfigSection(InternalCtorTag)
    : m_PreviousContext(g_CurrentContext)
{
    g_CurrentContext = this;
}

void ConfigSection::RestoreContext()
{
    g_CurrentContext = m_PreviousContext;
}

void ConfigSection::AddProperty(ConfigEntryBase& prop)
{
    m_Properties.push_back(&prop);
}

void ConfigSection::SectionToJSON(JSON& jsonOut)
{
    // Ensure we are outputting an object
    if (jsonOut.JSONType() != JSON::Class::Object)
        jsonOut = JSON::Make(JSON::Class::Object);

    for (auto& prop : m_Properties)
    {
        JSON& memberJSON = jsonOut[prop->m_Name];
        prop->ToJSON(memberJSON);
    }
}

bool ConfigSection::SectionFromJSON(JSON& jsonObject)
{
    if (jsonObject.JSONType() != JSON::Class::Object)
        return true; // Invalid structure implies dirty/rewrite needed

    bool missingOrInvalid = false;
    for (auto& prop : m_Properties)
    {
        if (jsonObject.hasKey(prop->m_Name))
        {
            JSON& memberJSON = jsonObject[prop->m_Name];
            if (!prop->FromJSON(memberJSON))
            {
                missingOrInvalid = true;
            }
        }
        else
        {
            missingOrInvalid = true;
        }
    }
    return missingOrInvalid;
}

}