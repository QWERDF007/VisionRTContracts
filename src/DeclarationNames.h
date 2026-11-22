#pragma once

#include "JsonDocument.h"
#include "visionrt/contracts/Declaration.h"

#include <type_traits>

namespace visionrt::decljson {
namespace detail {
// Filtering by the concrete enum keeps the wire names independent of the underlying ABI numbers.
template<class Enum, class Visit>
void visitDeclarationNames(Visit visit)
{
#define VRT_DECLARATION_NAME(group, member, spelling)         \
    if constexpr (std::is_same_v<Enum, group>)                \
    {                                                         \
        if (visit(group::member, std::string_view{spelling})) \
            return;                                           \
    }
#include "visionrt/contracts/DeclarationNames.def"
#undef VRT_DECLARATION_NAME
}
} // namespace detail

template<class Enum>
const char *wireName(Enum value)
{
    const char *result = nullptr;
    detail::visitDeclarationNames<Enum>(
        [&](Enum item, std::string_view name)
        {
            if (item != value)
                return false;
            result = name.data();
            return true;
        });
    if (!result)
        throw ContractError("未知声明枚举");
    return result;
}

template<class Enum>
Enum enumeration(const Node &node, std::string_view where)
{
    std::optional<Enum> result;
    if (node.kind == Kind::String)
        detail::visitDeclarationNames<Enum>(
            [&](Enum item, std::string_view name)
            {
                if (node.text != name)
                    return false;
                result = item;
                return true;
            });
    if (!result)
        throw ContractError(std::string(where) + "：未知枚举名称 " + node.text);
    return *result;
}
} // namespace visionrt::decljson
