#include "visionrt/contracts/TypeSpec.h"

#include <array>
#include <charconv>
#include <limits>

namespace visionrt {

const char *typeName(Type type)
{
    static constexpr std::array names{"Int", "Float", "Bool", "String", "Image", "Array", "ROI"};
    return names.at(static_cast<std::size_t>(type));
}

const char *typeLabel(Type type)
{
    static constexpr std::array names{"整数", "浮点数", "布尔值", "字符串", "图像", "数组", "ROI"};
    return names.at(static_cast<std::size_t>(type));
}

const char *dtypeName(DType dtype)
{
    static constexpr std::array names{"uint8", "int32", "int64", "float32", "float64", "bool", "string"};
    return names.at(static_cast<std::size_t>(dtype));
}

void validateTypeSpec(const TypeSpec &type)
{
    try
    {
        (void)typeName(type.type);
        if (type.dtype)
            (void)dtypeName(*type.dtype);
    }
    catch (const std::out_of_range &)
    {
        throw ContractError("Unknown port type or dtype");
    }
    if (type.type == Type::Array)
    {
        if (!type.dtype || type.rank < 1 || type.rank > kMaxArrayRank)
            throw ContractError("Array requires an explicit dtype and a supported rank in [1,"
                                + std::to_string(kMaxArrayRank) + "]");
    }
    else if (type.dtype || type.rank != -1)
    {
        throw ContractError("Only Array may declare dtype or rank");
    }
}

bool compatible(const TypeSpec &source, const TypeSpec &target)
{
    validateTypeSpec(source);
    validateTypeSpec(target);
    return source == target;
}

std::size_t checkedElementCount(std::span<const std::int64_t> shape)
{
    bool zero = false;
    for (const auto dimension : shape)
    {
        if (dimension < 0)
            throw ContractError("Shape contains a negative dimension");
        if (static_cast<std::uint64_t>(dimension) > std::numeric_limits<std::size_t>::max())
            throw ContractError("Dimension exceeds size_t");
        zero = zero || dimension == 0;
    }
    if (zero)
        return 0;
    std::size_t result = 1;
    for (const auto dimension : shape)
    {
        const auto size = static_cast<std::size_t>(dimension);
        if (result > std::numeric_limits<std::size_t>::max() / size)
            throw ContractError("Shape element count overflows size_t");
        result *= size;
    }
    return result;
}

std::size_t checkedByteCount(std::span<const std::int64_t> shape, std::size_t elementSize)
{
    if (elementSize == 0)
        throw ContractError("Element size must be positive");
    const auto count = checkedElementCount(shape);
    if (count > std::numeric_limits<std::size_t>::max() / elementSize)
        throw ContractError("Shape byte count overflows size_t");
    return count * elementSize;
}

std::int64_t parseInt64(std::string_view decimal)
{
    if (decimal.empty())
        throw ContractError("Expected an int64 decimal string");
    std::int64_t value  = 0;
    const auto   result = std::from_chars(decimal.data(), decimal.data() + decimal.size(), value, 10);
    if (result.ec == std::errc::result_out_of_range)
        throw ContractError("Decimal integer is outside the int64 range");
    if (result.ec != std::errc{} || result.ptr != decimal.data() + decimal.size())
        throw ContractError("Expected only an optional minus and int64 decimal digits");
    return value;
}

std::string formatInt64(std::int64_t value)
{
    std::array<char, 20> text{};
    const auto           result = std::to_chars(text.data(), text.data() + text.size(), value);
    return std::string(text.data(), result.ptr);
}

void validateUtf8(std::string_view value)
{
    for (std::size_t index = 0; index < value.size();)
    {
        const auto first = static_cast<unsigned char>(value[index++]);
        if (first < 0x80)
            continue;
        std::uint32_t code      = 0;
        std::uint32_t minimum   = 0;
        std::size_t   remaining = 0;
        if (first >= 0xc2 && first <= 0xdf)
        {
            code      = first & 0x1f;
            minimum   = 0x80;
            remaining = 1;
        }
        else if (first >= 0xe0 && first <= 0xef)
        {
            code      = first & 0x0f;
            minimum   = 0x800;
            remaining = 2;
        }
        else if (first >= 0xf0 && first <= 0xf4)
        {
            code      = first & 0x07;
            minimum   = 0x10000;
            remaining = 3;
        }
        else
        {
            throw ContractError("String contains invalid UTF-8");
        }
        if (remaining > value.size() - index)
            throw ContractError("String contains truncated UTF-8");
        while (remaining-- != 0)
        {
            const auto next = static_cast<unsigned char>(value[index++]);
            if ((next & 0xc0) != 0x80)
                throw ContractError("String contains invalid UTF-8 continuation");
            code = (code << 6) | (next & 0x3f);
        }
        if (code < minimum || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff))
            throw ContractError("String contains an invalid UTF-8 code point");
    }
}

}
