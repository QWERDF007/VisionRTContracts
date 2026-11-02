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

namespace {
constexpr std::string_view kVecPrefix = "vec_";

// Element spellings: the compact token is the only place where a dtype name appears, so a
// declaration never repeats dtype and rank in two fields.
struct ElementToken
{
    std::string_view token;
    DType            dtype;
};

constexpr std::array<ElementToken, 7> kElements{{
    {   "uint8",  DType::UInt8},
    {   "int32",  DType::Int32},
    {     "int",  DType::Int64},
    { "float32", DType::Float32},
    {   "float", DType::Float64},
    {    "bool",   DType::Bool},
    {  "string", DType::String},
}};

const ElementToken &elementToken(std::string_view token)
{
    for (const auto &element : kElements)
        if (element.token == token)
            return element;
    throw ContractError("未知数组元素类型：" + std::string(token));
}

std::string_view elementTokenValue(DType dtype)
{
    for (const auto &element : kElements)
        if (element.dtype == dtype)
            return element.token;
    throw ContractError("未知数组 dtype");
}
} // namespace

TypeSpec parseTypeToken(std::string_view token)
{
    int          rank = 0;
    std::string_view base = token;
    while (base.starts_with(kVecPrefix))
    {
        ++rank;
        base.remove_prefix(kVecPrefix.size());
    }
    if (rank > kMaxArrayRank)
        throw ContractError("数组 token 最多四层 vec_：" + std::string(token));
    if (rank == 0)
    {
        if (base == "int")
            return {Type::Int};
        if (base == "float")
            return {Type::Float};
        if (base == "bool")
            return {Type::Bool};
        if (base == "string")
            return {Type::String};
        if (base == "image")
            return {Type::Image};
        if (base == "roi")
            return {Type::ROI};
        throw ContractError("未知端口类型 token：" + std::string(token));
    }
    return {Type::Array, elementToken(base).dtype, rank};
}

std::string typeToken(const TypeSpec &type)
{
    validateTypeSpec(type);
    switch (type.type)
    {
    case Type::Int:
        return "int";
    case Type::Float:
        return "float";
    case Type::Bool:
        return "bool";
    case Type::String:
        return "string";
    case Type::Image:
        return "image";
    case Type::ROI:
        return "roi";
    case Type::Array:
        break;
    }
    std::string token;
    for (int level = 0; level < type.rank; ++level) token.append(kVecPrefix);
    token.append(elementTokenValue(*type.dtype));
    return token;
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
