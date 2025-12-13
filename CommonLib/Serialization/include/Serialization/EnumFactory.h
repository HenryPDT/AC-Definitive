#pragma once

#include <optional>
#include <string_view>
#include <array>

/*
Static reflection for enums using macros.
*/

template<typename EnumType>
struct enum_reflection {
    static constexpr const char* GetString(EnumType value);
    static constexpr std::optional<EnumType> GetValue(const std::string_view& str);
    static constexpr size_t GetNumItems();
    static constexpr bool Contains(long long value);
    static constexpr auto GetAllStrings();
    static constexpr auto GetAllValues();
};

constexpr const std::string_view INVALID_ENUM_TO_STRING = "INVALID_TO_STRING";

// Expansion macros
#define ENUM_VALUE(name,assign) name assign,
#define ENUM_CASE(name,assign) case current_enum_type::name: return #name;
#define ENUM_STRCMP(name,assign) if (str == #name) return current_enum_type::name;
#define ENUM_COUNTER(name,assign) "1"
#define ENUM_CONTAINS(name,assign) case static_cast<long long>(current_enum_type::name): return true;
#define ENUM_STRING_AND_COMMA(name,assign) #name,
#define ENUM_VALUE_AND_COMMA(name,assign) current_enum_type::name,

#define DECLARE_ENUM(EnumType,ENUM_DEF) \
  enum class EnumType { \
    ENUM_DEF(ENUM_VALUE) \
  }; \
  template<> \
  constexpr const char *enum_reflection<EnumType>::GetString(EnumType value) \
  { \
    using current_enum_type = EnumType; \
    switch(value) \
    { \
      ENUM_DEF(ENUM_CASE) \
      default: return INVALID_ENUM_TO_STRING.data(); \
    } \
  } \
  template<> \
  constexpr std::optional<EnumType> enum_reflection<EnumType>::GetValue(const std::string_view& str) \
  { \
    using current_enum_type = EnumType; \
    ENUM_DEF(ENUM_STRCMP) \
    return {}; \
  } \
  template<> \
  constexpr size_t enum_reflection<EnumType>::GetNumItems() \
  { \
    constexpr std::string_view one_char_for_every_element = ENUM_DEF(ENUM_COUNTER); \
    return one_char_for_every_element.size(); \
  } \
  template<> \
  constexpr bool enum_reflection<EnumType>::Contains(long long value) \
  { \
    using current_enum_type = EnumType; \
    switch(value) \
    { \
      ENUM_DEF(ENUM_CONTAINS) \
      default: return false; \
    } \
  } \
  template<> \
  constexpr auto enum_reflection<EnumType>::GetAllStrings() \
  { \
    constexpr std::array<const char*, GetNumItems()> result = { ENUM_DEF(ENUM_STRING_AND_COMMA) }; \
    return result; \
  } \
  template<> \
  constexpr auto enum_reflection<EnumType>::GetAllValues() \
  { \
    using current_enum_type = EnumType; \
    constexpr std::array<EnumType, GetNumItems()> result = { ENUM_DEF(ENUM_VALUE_AND_COMMA) }; \
    return result; \
  }
