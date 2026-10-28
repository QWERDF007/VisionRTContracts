#pragma once

#include "visionrt/contracts/Export.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace visionrt {

enum class Type
{
    Int,
    Float,
    Bool,
    String,
    Image,
    Array,
    ROI
};
enum class DType
{
    UInt8,
    Int32,
    Int64,
    Float32,
    Float64,
    Bool,
    String
};

inline constexpr int kMaxArrayRank = 4;

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

// The wire token ("Int", "Array", …) used by declarations and JSON.
VT_EXPORT const char *typeName(Type type);

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
