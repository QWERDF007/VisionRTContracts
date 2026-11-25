#pragma once

#include "visionrt/contracts/Export.h"

#include <cstddef>
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace visionrt {

#define VRT_ENUM_FILE "TypeSpec.def"
#include <visionrt/contracts/EnumDeclarations.h>

#define VRT_ENUM_BEGIN(name, underlying, ...) inline constexpr auto k##name##Values = [] { using Enum = name; return std::to_array<Enum>({
#define VRT_ENUM_VALUE(name, value, ...) Enum::name,
#define VRT_ENUM_END(name) }); }();
#include "TypeSpec.def"
#undef VRT_ENUM_BEGIN
#undef VRT_ENUM_VALUE
#undef VRT_ENUM_END

inline constexpr int kMaxArrayRank = 4;
constexpr bool isShapeType(Type type) noexcept
{
    switch (type)
    {
#define VRT_SHAPE(name, kind, view, code, wire, label) case Type::name: return true;
#include "ShapeKinds.def"
#undef VRT_SHAPE
    default: return false;
    }
}
constexpr bool isGeometryType(Type type) noexcept
{
    return isShapeType(type) || type == Type::Direction || type == Type::CoordinateFrame2D;
}

struct TypeSpec
{
    Type                 type = Type::Int;
    std::optional<DType> dtype;
    int                  rank                               = -1;
    bool                 operator==(const TypeSpec &) const = default;
};

class ContractError : public std::invalid_argument
{
public:
    using std::invalid_argument::invalid_argument;
};

VT_EXPORT const char *typeName(Type type);
VT_EXPORT Type typeFromCode(std::int64_t code);

// The same type as a reader of a message expects to see it ("整数", "浮点数", …).
VT_EXPORT const char *typeLabel(Type type);
VT_EXPORT const char *dtypeName(DType dtype);
VT_EXPORT void        validateTypeSpec(const TypeSpec &type);
VT_EXPORT bool        compatible(const TypeSpec &source, const TypeSpec &target);
VT_EXPORT std::size_t checkedElementCount(std::span<const std::int64_t> shape);
VT_EXPORT std::size_t checkedByteCount(std::span<const std::int64_t> shape, std::size_t elementSize);
VT_EXPORT std::int64_t parseInt64(std::string_view decimal);
VT_EXPORT std::string formatInt64(std::int64_t value);
VT_EXPORT void        validateUtf8(std::string_view text);

}
