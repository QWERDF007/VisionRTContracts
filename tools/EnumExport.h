#pragma once

#include <cstdint>
#include <initializer_list>
#include <span>
#include <string_view>

namespace visionrt::codegen {

struct EnumMember
{
    std::string_view name;
    std::int64_t value;
    std::string_view pythonName{};
    std::string_view message{};
};

struct EnumDefinition
{
    std::string_view name;
    std::string_view header;
    std::span<const EnumMember> members;
    std::string_view pythonName{};
    std::string_view messageMap{};
};

struct WireName
{
    std::string_view enumName;
    std::string_view member;
    std::string_view wire;
};

int run(int argc, const char *const *argv,
        std::initializer_list<std::span<const EnumDefinition>> catalogs,
        std::span<const WireName> wireNames = {});

#ifdef _WIN32
int run(int argc, const wchar_t *const *argv,
        std::initializer_list<std::span<const EnumDefinition>> catalogs,
        std::span<const WireName> wireNames = {});
#endif

} // namespace visionrt::codegen
