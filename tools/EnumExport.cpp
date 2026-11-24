#include "EnumExport.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace visionrt::codegen {
namespace {

std::string pythonIdentifier(std::string_view name)
{
    constexpr std::string_view keywords[]{"False", "None", "True", "and", "as", "assert", "async",
        "await", "break", "class", "continue", "def", "del", "elif", "else", "except", "finally",
        "for", "from", "global", "if", "import", "in", "is", "lambda", "nonlocal", "not", "or",
        "pass", "raise", "return", "try", "while", "with", "yield"};
    return std::string(name) + (std::find(std::begin(keywords), std::end(keywords), name)
                                   != std::end(keywords) ? "_" : "");
}

std::string quoted(std::string_view text)
{
    std::string result = "\"";
    constexpr char hex[] = "0123456789abcdef";
    for (const unsigned char value : text)
    {
        switch (value)
        {
        case '\\': result += "\\\\"; break;
        case '"': result += "\\\""; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default:
            if (value < 32)
            {
                result += "\\x";
                result += hex[value >> 4];
                result += hex[value & 15];
            }
            else
                result += static_cast<char>(value);
        }
    }
    return result + '"';
}

} // namespace

int run(int argc, const char *const *argv,
        std::initializer_list<std::span<const EnumDefinition>> catalogs,
        std::span<const WireName> wireNames)
try
{
    if (argc != 3 || (std::string_view(argv[1]) != "--qml" && std::string_view(argv[1]) != "--python"))
        throw std::invalid_argument("usage: enum_export --qml <output> | --python <output>");
    const bool python = std::string_view(argv[1]) == "--python";
    std::ostringstream source;
    std::set<std::string_view> headers;
    std::map<std::string_view, const EnumDefinition *> definitions;
    for (const auto catalog : catalogs)
        for (const auto &definition : catalog)
        {
            if (!definitions.emplace(definition.name, &definition).second)
                throw std::invalid_argument("duplicate enum: " + std::string(definition.name));
            headers.insert(definition.header);
        }
    if (python)
        source << "from enum import IntEnum\n\n";
    else
    {
        source << "#pragma once\n#include <QObject>\n";
        for (const auto header : headers)
            source << "#include " << quoted(header) << '\n';
        source << "namespace visionrt::qml {\n";
    }
    for (const auto catalog : catalogs)
        for (const auto &definition : catalog)
        {
            const auto name = definition.pythonName.empty() ? definition.name : definition.pythonName;
            if (python)
                source << "class " << name << "(IntEnum):\n";
            else
                source << "class " << definition.name << " {\n    Q_GADGET\npublic:\n    enum Value {\n";
            for (const auto &member : definition.members)
            {
                if (python)
                    source << "    " << pythonIdentifier(member.pythonName.empty() ? member.name : member.pythonName)
                           << " = " << member.value << '\n';
                else
                    source << "        " << member.name << " = static_cast<int>(visionrt::"
                           << definition.name << "::" << member.name << "),\n";
            }
            if (!python)
                source << "    };\n    Q_ENUM(Value)\n};\n";
            source << '\n';
            if (python && !definition.messageMap.empty())
            {
                source << definition.messageMap << " = {\n";
                for (const auto &member : definition.members)
                    source << "    " << member.value << ": " << quoted(member.message) << ",\n";
                source << "}\n\n";
            }
        }
    if (python)
    {
        source << "ENUM_WIRE_NAMES = {\n";
        std::map<std::string_view, std::vector<const WireName *>> grouped;
        for (const auto &wire : wireNames)
            if (definitions.contains(wire.enumName))
                grouped[wire.enumName].push_back(&wire);
        for (const auto &[enumName, names] : grouped)
        {
            const auto &definition = *definitions.at(enumName);
            source << "    " << quoted(enumName) << ": {\n";
            for (const auto *wire : names)
            {
                const auto member = std::find_if(definition.members.begin(), definition.members.end(),
                    [&](const auto &item) { return item.name == wire->member; });
                if (member == definition.members.end())
                    throw std::invalid_argument("unknown enum member: " + std::string(wire->member));
                source << "        " << quoted(wire->wire) << ": "
                       << (definition.pythonName.empty() ? definition.name : definition.pythonName) << '.'
                       << pythonIdentifier(member->pythonName.empty() ? member->name : member->pythonName) << ",\n";
            }
            source << "    },\n";
        }
        source << "}\n";
    }
    else
        source << "}\n";
    const std::string_view outputName(argv[2]);
    const std::filesystem::path output(std::u8string_view(
        reinterpret_cast<const char8_t *>(outputName.data()), outputName.size()));
    const auto text = source.str();
    {
        std::ifstream existing(output, std::ios::binary);
        if (existing && std::string(std::istreambuf_iterator<char>(existing), {}) == text)
            return 0;
    }
    if (output.has_parent_path())
        std::filesystem::create_directories(output.parent_path());
    std::ofstream file(output, std::ios::binary | std::ios::trunc);
    file.exceptions(std::ios::failbit | std::ios::badbit);
    file << text;
    file.close();
    return 0;
}
catch (const std::exception &error)
{
    std::cerr << "enum_export: " << error.what() << '\n';
    return 1;
}

#ifdef _WIN32
int run(int argc, const wchar_t *const *argv,
        std::initializer_list<std::span<const EnumDefinition>> catalogs,
        std::span<const WireName> wireNames)
try
{
    std::vector<std::string> arguments;
    for (int i = 0; i < argc; ++i)
    {
        const auto text = std::filesystem::path(argv[i]).u8string();
        arguments.emplace_back(reinterpret_cast<const char *>(text.data()), text.size());
    }
    std::vector<const char *> pointers;
    for (const auto &argument : arguments)
        pointers.push_back(argument.c_str());
    return run(argc, pointers.data(), catalogs, wireNames);
}
catch (const std::exception &error)
{
    std::cerr << "enum_export: " << error.what() << '\n';
    return 1;
}
#endif

} // namespace visionrt::codegen
