#include "DeclarationValue.h"

#include "DeclarationNames.h"
#include "visionrt/contracts/Issue.h"

#include <charconv>
#include <cmath>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>

namespace visionrt::decljson {
namespace {

std::string whatText(std::string_view what)
{
    return std::string(what);
}

double real(const Node &node, std::string_view what)
{
    if (node.kind != Kind::Number)
        throw ContractError(whatText(what) + " 必须是数值");
    double value = 0;
    const auto result = std::from_chars(node.text.data(), node.text.data() + node.text.size(), value);
    if (result.ec != std::errc{} || result.ptr != node.text.data() + node.text.size() || !std::isfinite(value))
        throw ContractError(whatText(what) + " 必须是有限数值");
    return value;
}

bool boolean(const Node &node, std::string_view what)
{
    if (node.kind != Kind::Bool)
        throw ContractError(whatText(what) + " 必须是布尔值");
    return node.boolean;
}

std::string string(const Node &node, std::string_view what)
{
    if (node.kind != Kind::String)
        throw ContractError(whatText(what) + " 必须是字符串");
    validateUtf8(node.text);
    return node.text;
}

const std::vector<Node> &array(const Node &node, std::string_view what)
{
    if (node.kind != Kind::Array)
        throw ContractError(whatText(what) + " 必须是数组");
    return node.items;
}

const Node &required(const Node &node, std::string_view name)
{
    if (const auto *field = node.find(name)) return *field;
    throw ContractError("缺少字段：" + std::string(name));
}

std::uint32_t identifier(const Node &node)
{
    const auto value = integer(node, "编号");
    if (value < 0 || value > std::numeric_limits<std::uint32_t>::max()) throw ContractError("编号超出支持范围");
    return static_cast<std::uint32_t>(value);
}

cv::Mat readPixels(const Node &node)
{
    try
    {
        cv::FileStorage storage(string(node, "图像"), cv::FileStorage::READ | cv::FileStorage::MEMORY);
        cv::Mat pixels;
        storage["pixels"] >> pixels;
        if (pixels.empty()) throw ContractError("图像像素为空");
        return pixels;
    }
    catch (const cv::Exception &error) { throw ContractError("图像存储格式不正确：" + std::string(error.what())); }
}

void writePixels(Writer &writer, const cv::Mat &pixels)
{
    cv::FileStorage storage(".json", cv::FileStorage::WRITE | cv::FileStorage::MEMORY
                                    | cv::FileStorage::FORMAT_JSON | cv::FileStorage::BASE64);
    storage << "pixels" << pixels;
    const auto json = storage.releaseAndGetString();
    writer.String(json.c_str(), static_cast<rapidjson::SizeType>(json.size()));
}

TrainingSet readTrainingSet(const Node &node)
{
    TrainingSet set;
    for (const auto &image : array(required(node, "images"), "训练图像"))
    {
        const auto id = identifier(required(image, "id"));
        const auto order = enumeration<ColorOrder>(required(image, "color_order"), "color_order");
        if (!set.images.emplace(id, TrainingImage{id, readPixels(required(image, "pixels")), order}).second)
            throw ContractError("训练图像编号重复");
    }
    for (const auto &record : array(required(node, "records"), "训练记录"))
    {
        TrainingRecord r;
        r.id = identifier(required(record, "id"));
        const auto &image = required(record, "image");
        if (image.kind != Kind::Null) r.imageId = identifier(image);
        for (const auto &field : array(required(record, "fields"), "记录字段"))
        {
            const auto id = identifier(required(field, "id"));
            const auto &encodedType = required(field, "type");
            const auto &encodedValue = required(field, "value");
            if (encodedType.kind == Kind::Null && encodedValue.kind != Kind::Null)
                throw ContractError("缺失字段值必须为 null");
            auto value = encodedType.kind == Kind::Null ? PortValue{Missing{}} : decodeValue(encodedValue, decodeType(encodedType));
            if (!r.fields.emplace(id, std::move(value)).second)
                throw ContractError("训练字段编号重复");
        }
        if (!set.records.emplace(r.id, std::move(r)).second) throw ContractError("训练记录编号重复");
    }
    return set;
}

void writeTrainingSet(Writer &writer, const TrainingSet &set)
{
    writer.StartObject();
    writer.Key("images"); writer.StartArray();
    for (const auto &[id, image] : set.images)
    {
        writer.StartObject();
        writer.Key("id"); writer.Uint(id);
        writer.Key("color_order"); writer.String(wireName(image.colorOrder));
        writer.Key("pixels"); writePixels(writer, image.pixels);
        writer.EndObject();
    }
    writer.EndArray();
    writer.Key("records"); writer.StartArray();
    for (const auto &[id, r] : set.records)
    {
        writer.StartObject();
        writer.Key("id"); writer.Uint(id);
        writer.Key("image"); if (r.imageId) writer.Uint(*r.imageId); else writer.Null();
        writer.Key("fields"); writer.StartArray();
        for (const auto &[field, value] : r.fields)
        {
            writer.StartObject();
            writer.Key("id"); writer.Uint(field);
            writer.Key("type"); if (isMissing(value)) writer.Null(); else encodeType(writer, *typeOf(value));
            writer.Key("value"); if (isMissing(value)) writer.Null(); else encodeValue(writer, value, *typeOf(value));
            writer.EndObject();
        }
        writer.EndArray(); writer.EndObject();
    }
    writer.EndArray(); writer.EndObject();
}

template<class Element>
Element element(const Node &node, std::string_view what)
{
    if constexpr (std::is_same_v<Element, bool>)
        return boolean(node, what);
    else if constexpr (std::is_same_v<Element, std::string>)
        return string(node, what);
    else if constexpr (std::is_integral_v<Element>)
    {
        const auto value = integer(node, what);
        if (value < std::numeric_limits<Element>::min() || value > std::numeric_limits<Element>::max())
            throw ContractError(whatText(what) + " 超出声明的整数范围");
        return static_cast<Element>(value);
    }
    else
    {
        const auto value = static_cast<Element>(real(node, what));
        if (!std::isfinite(value))
            throw ContractError(whatText(what) + " 超出声明的浮点范围");
        return value;
    }
}

template<class Vector>
Vector decodeNested(const Node &node, std::string_view what)
{
    using Element = typename Vector::value_type;
    const auto &entries = array(node, what);
    Vector      result;
    result.reserve(entries.size());
    for (const auto &entry : entries)
    {
        if constexpr (std::is_arithmetic_v<Element> || std::is_same_v<Element, std::string>)
            result.push_back(element<Element>(entry, what));
        else
            result.push_back(decodeNested<Element>(entry, what));
    }
    return result;
}

template<class T>
std::vector<T> arrayValues(const Node &node, std::string_view what)
{
    std::vector<T> result;
    const auto    &values = array(node, what);
    result.reserve(values.size());
    for (const auto &value : values)
        result.push_back(element<T>(value, what));
    return result;
}

// One leaf of a typed array. The dtype decides how the text is interpreted, so an Int64 array keeps
// every digit and a bool array never accepts 0/1 in its place.
template<class T>
void writeLeaf(Writer &writer, const Array &array, std::size_t index)
{
    if constexpr (std::is_same_v<T, bool>)
        writer.Bool(array.boolBytes()[index] != 0);
    else if constexpr (std::is_same_v<T, std::string>)
    {
        const auto &text = array.elements<std::string>()[index];
        writer.String(text.c_str(), static_cast<rapidjson::SizeType>(text.size()));
    }
    else if constexpr (std::is_same_v<T, std::uint8_t>)
        writer.Uint(array.elements<T>()[index]);
    else if constexpr (std::is_integral_v<T>)
        writer.Int64(static_cast<std::int64_t>(array.elements<T>()[index]));
    else
        writer.Double(static_cast<double>(array.elements<T>()[index]));
}

template<class T>
void writeDense(Writer &writer, const Array &array, int dimension, std::size_t &index)
{
    writer.StartArray();
    const auto &shape = array.shape();
    for (std::int64_t i = 0; i < shape[static_cast<std::size_t>(dimension)]; ++i)
    {
        if (dimension + 1 == static_cast<int>(shape.size()))
            writeLeaf<T>(writer, array, index++);
        else
            writeDense<T>(writer, array, dimension + 1, index);
    }
    writer.EndArray();
}

template<class T>
void writeRagged(Writer &writer, const Array &array, int level, std::size_t group, std::size_t &index)
{
    writer.StartArray();
    const auto &bounds = array.offsets()[static_cast<std::size_t>(level)];
    if (level + 1 == static_cast<int>(array.offsets().size()))
    {
        for (auto leaf = bounds[group]; leaf < bounds[group + 1]; ++leaf)
            writeLeaf<T>(writer, array, index++);
        writer.EndArray();
        return;
    }
    for (auto child = bounds[group]; child < bounds[group + 1]; ++child)
        writeRagged<T>(writer, array, level + 1, static_cast<std::size_t>(child), index);
    writer.EndArray();
}

template<class T>
void writeArrayTyped(Writer &writer, const Array &array)
{
    if (array.layout() == ArrayLayout::Dense)
    {
        std::size_t index = 0;
        writeDense<T>(writer, array, 0, index);
        return;
    }
    std::size_t index  = 0;
    const auto  groups = array.offsets().empty() ? std::size_t{0} : array.offsets()[0].size() - 1;
    writer.StartArray();
    for (std::size_t group = 0; group < groups; ++group)
        writeRagged<T>(writer, array, 0, group, index);
    writer.EndArray();
}

void writeArray(Writer &writer, const Array &array)
{
    switch (array.dtype())
    {
    case DType::UInt8:
        return writeArrayTyped<std::uint8_t>(writer, array);
    case DType::Int32:
        return writeArrayTyped<std::int32_t>(writer, array);
    case DType::Int64:
        return writeArrayTyped<std::int64_t>(writer, array);
    case DType::Float32:
        return writeArrayTyped<float>(writer, array);
    case DType::Float64:
        return writeArrayTyped<double>(writer, array);
    case DType::Bool:
        return writeArrayTyped<bool>(writer, array);
    case DType::String:
        return writeArrayTyped<std::string>(writer, array);
    }
    throw ContractError("未知数组 dtype");
}

template<class T>
Array typedNested(const Node &node, int rank)
{
    switch (rank)
    {
    case 1:
        return Array::fromNested(decodeNested<NestedVector<T, 1>>(node, "数组元素"));
    case 2:
        return Array::fromNested(decodeNested<NestedVector<T, 2>>(node, "数组元素"));
    case 3:
        return Array::fromNested(decodeNested<NestedVector<T, 3>>(node, "数组元素"));
    case 4:
        return Array::fromNested(decodeNested<NestedVector<T, 4>>(node, "数组元素"));
    }
    throw ContractError("数组 rank 必须在支持范围内");
}

template<class T>
Array typedStructured(const Node &node, int rank, DType dtype)
{
    for (const auto &[name, value] : node.members)
        if (name != "shape" && name != "offsets" && name != "values")
            throw ContractError("数组结构化值包含未知字段：" + name);
    const auto *values = node.find("values");
    if (values == nullptr)
        throw ContractError("数组结构化值缺少 values");
    const auto elements = arrayValues<T>(*values, "数组元素");
    if (const auto *shape = node.find("shape"))
    {
        if (node.has("offsets"))
            throw ContractError("数组结构化值不能同时包含 shape 与 offsets");
        std::vector<std::int64_t> sizes;
        for (const auto &value : array(*shape, "shape"))
            sizes.push_back(integer(value, "shape"));
        auto result = Array::dense(dtype, std::move(sizes), elements);
        if (result.rank() != rank)
            throw ContractError("数组 shape 的 rank 与声明不一致");
        return result;
    }
    if (const auto *offsets = node.find("offsets"))
    {
        std::vector<std::vector<std::int64_t>> bounds;
        for (const auto &level : array(*offsets, "offsets"))
        {
            std::vector<std::int64_t> numbers;
            for (const auto &value : array(level, "offsets"))
                numbers.push_back(integer(value, "offsets"));
            bounds.push_back(std::move(numbers));
        }
        return Array::ragged(dtype, rank, std::move(bounds), elements);
    }
    throw ContractError("数组结构化值必须包含 shape 或 offsets");
}

Array decodeArray(const Node &node, DType dtype, int rank)
{
    if (node.kind == Kind::Object)
    {
        switch (dtype)
        {
        case DType::UInt8:
            return typedStructured<std::uint8_t>(node, rank, dtype);
        case DType::Int32:
            return typedStructured<std::int32_t>(node, rank, dtype);
        case DType::Int64:
            return typedStructured<std::int64_t>(node, rank, dtype);
        case DType::Float32:
            return typedStructured<float>(node, rank, dtype);
        case DType::Float64:
            return typedStructured<double>(node, rank, dtype);
        case DType::Bool:
            return typedStructured<bool>(node, rank, dtype);
        case DType::String:
            return typedStructured<std::string>(node, rank, dtype);
        }
        throw ContractError("未知数组 dtype");
    }
    switch (dtype)
    {
    case DType::UInt8:
        return typedNested<std::uint8_t>(node, rank);
    case DType::Int32:
        return typedNested<std::int32_t>(node, rank);
    case DType::Int64:
        return typedNested<std::int64_t>(node, rank);
    case DType::Float32:
        return typedNested<float>(node, rank);
    case DType::Float64:
        return typedNested<double>(node, rank);
    case DType::Bool:
        return typedNested<bool>(node, rank);
    case DType::String:
        return typedNested<std::string>(node, rank);
    }
    throw ContractError("未知数组 dtype");
}

} // namespace

std::int64_t integer(const Node &node, std::string_view what)
{
    if (node.kind != Kind::Number)
        throw ContractError(whatText(what) + " 必须是整数");
    if (node.text.find_first_of(".eE") != std::string::npos)
        throw ContractError(whatText(what) + " 必须是整数，不能写成小数或指数");
    try
    {
        return parseInt64(node.text);
    }
    catch (const ContractError &)
    {
        throw ContractError(whatText(what) + " 超出 int64 范围");
    }
}

TypeSpec decodeType(const Node &node)
{
    if (node.kind == Kind::String)
    {
        TypeSpec spec{enumeration<Type>(node, "type")};
        validateTypeSpec(spec);
        return spec;
    }
    if (node.kind != Kind::Object)
        throw ContractError("类型必须是名称或数组描述");
    for (const auto &[name, value] : node.members)
        if (name != "type" && name != "dtype" && name != "rank")
            throw ContractError("TypeSpec 包含未知字段：" + name);
    const auto *type = node.find("type");
    const auto *dtype = node.find("dtype");
    if (!type || enumeration<Type>(*type, "type") != Type::Array || !dtype)
        throw ContractError("数组描述必须指定 array、dtype 和 rank");
    TypeSpec spec{Type::Array, enumeration<DType>(*dtype, "dtype")};
    if (const auto *rank = node.find("rank"))
    {
        const auto value = integer(*rank, "rank");
        if (value < 1 || value > kMaxArrayRank)
            throw ContractError("数组 rank 必须在支持范围内");
        spec.rank = static_cast<int>(value);
    }
    validateTypeSpec(spec);
    return spec;
}

void encodeType(Writer &writer, const TypeSpec &type)
{
    validateTypeSpec(type);
    if (type.type != Type::Array)
    {
        writer.String(wireName(type.type));
        return;
    }
    writer.StartObject();
    writer.Key("type");
    writer.String(wireName(type.type));
    if (type.type == Type::Array)
    {
        writer.Key("dtype");
        writer.String(wireName(*type.dtype));
        writer.Key("rank");
        writer.Int(type.rank);
    }
    writer.EndObject();
}

PortValue decodeValue(const Node &node, const TypeSpec &type)
{
    validateTypeSpec(type);
    if (node.kind == Kind::Null)
        return Missing{};
    PortValue result;
    switch (type.type)
    {
    case Type::Int:
        result = integer(node, "值");
        break;
    case Type::Float:
        result = real(node, "值");
        break;
    case Type::Bool:
        result = boolean(node, "值");
        break;
    case Type::String:
        result = string(node, "值");
        break;
    case Type::Image:
    {
        Image images;
        for (const auto &image : array(node, "图像批次")) images.push_back(readPixels(image));
        result = DataRef<Image>(std::move(images));
        break;
    }
    case Type::TrainingSet:
        result = DataRef<TrainingSet>(readTrainingSet(node));
        break;
    case Type::Array:
        result = decodeArray(node, *type.dtype, type.rank);
        break;
#define VRT_SHAPE(name, kind, view, code, wire, label) case Type::name: result = name::fromNested(decodeNested<ROI>(node, label)); break;
#include "visionrt/contracts/ShapeKinds.def"
#undef VRT_SHAPE
    case Type::Direction:
        result = Direction(decodeArray(node, *Direction::storageType().dtype, Direction::storageType().rank));
        break;
    case Type::CoordinateFrame2D:
        result = CoordinateFrame2D(decodeArray(node, *CoordinateFrame2D::storageType().dtype, CoordinateFrame2D::storageType().rank));
        break;
    }
    validateValue(type, result);
    return result;
}

void encodeValue(Writer &writer, const PortValue &value, const TypeSpec &type)
{
    validateValue(type, value);
    std::visit(
        [&](const auto &item)
        {
            using Item = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<Item, Missing>)
                writer.Null();
            else if constexpr (std::is_same_v<Item, std::int64_t>)
                writer.Int64(item);
            else if constexpr (std::is_same_v<Item, double>)
            {
                if (!std::isfinite(item))
                    throw ContractError("JSON 字面量不支持 NaN 或 Infinity");
                writer.Double(item);
            }
            else if constexpr (std::is_same_v<Item, bool>)
                writer.Bool(item);
            else if constexpr (std::is_same_v<Item, std::string>)
                writer.String(item.c_str(), static_cast<rapidjson::SizeType>(item.size()));
            else if constexpr (std::is_same_v<Item, Array>)
                writeArray(writer, item);
            else if constexpr (std::is_same_v<Item, Direction> || std::is_same_v<Item, CoordinateFrame2D>)
                writeArray(writer, item.array());
            else if constexpr (isShapeValue<Item>)
            {
                const auto roi = item.nested();
                writer.StartArray();
                for (const auto &slot : roi)
                {
                    writer.StartArray();
                    for (const auto &region : slot)
                    {
                        writer.StartArray();
                        for (const auto number : region)
                            writer.Double(number);
                        writer.EndArray();
                    }
                    writer.EndArray();
                }
                writer.EndArray();
            }
            else if constexpr (std::is_same_v<Item, DataRef<Image>>)
            {
                writer.StartArray();
                for (const auto &image : item.get()) writePixels(writer, image);
                writer.EndArray();
            }
            else
                writeTrainingSet(writer, item.get());
        },
        value);
}

} // namespace visionrt::decljson
