#include "visionrt/contracts/Value.h"

#include <algorithm>
#include <limits>
#include <type_traits>

namespace visionrt {

void validateImageFrame(const cv::Mat &image)
{
    if (image.dims != 2 || image.empty())
        throw ContractError("Image must be a nonempty two-dimensional OpenCV matrix");
    const auto rowBytes = checkedByteCount(std::array<std::int64_t, 1>{image.cols}, image.elemSize());
    if (image.step[1] != image.elemSize() || image.step[0] < rowBytes)
        throw ContractError("Image pixel/row strides exceed its buffer layout");
    const auto rows = static_cast<std::size_t>(image.rows - 1);
    if (rows != 0 && image.step[0] > (std::numeric_limits<std::size_t>::max() - rowBytes) / rows)
        throw ContractError("Image stride byte count overflows size_t");
    const auto bytes = rows * image.step[0] + rowBytes;
    if (!image.dataend || image.dataend < image.data ||
        bytes > static_cast<std::size_t>(image.dataend - image.data))
        throw ContractError("Image strides exceed backing buffer range");
}

bool isMissing(const PortValue &value) noexcept
{
    return std::holds_alternative<Missing>(value);
}

const ROI *shapeRegions(const PortValue &value) noexcept
{
    return std::visit([](const auto &payload) -> const ROI * {
        using T = std::decay_t<decltype(payload)>;
        if constexpr (isShapeValue<T>) return &payload.get();
        else return nullptr;
    }, value);
}

PortValue shapeValue(Type type, ROI encoded)
{
    switch (type)
    {
#define VRT_SHAPE(name, kind, view, code, wire, label) case Type::name: return name::fromEncoded(std::move(encoded));
#include "visionrt/contracts/ShapeKinds.def"
#undef VRT_SHAPE
    default: throw ContractError("参数必须声明点、线、矩形、圆或多边形类型");
    }
}

std::optional<TypeSpec> typeOf(const PortValue &value)
{
    return std::visit(
        [](const auto &payload) -> std::optional<TypeSpec>
        {
            using T = std::decay_t<decltype(payload)>;
            if constexpr (std::is_same_v<T, Missing>)
                return {};
            else if constexpr (std::is_same_v<T, std::int64_t>)
                return TypeSpec{Type::Int};
            else if constexpr (std::is_same_v<T, double>)
                return TypeSpec{Type::Float};
            else if constexpr (std::is_same_v<T, bool>)
                return TypeSpec{Type::Bool};
            else if constexpr (std::is_same_v<T, std::string>)
                return TypeSpec{Type::String};
            else if constexpr (std::is_same_v<T, DataRef<Image>>)
                return TypeSpec{Type::Image};
            else if constexpr (std::is_same_v<T, DataRef<TrainingSet>>)
                return TypeSpec{Type::TrainingSet};
            else
                return payload.type();
        },
        value);
}

void validateValue(const TypeSpec &expected, const PortValue &value)
{
    validateTypeSpec(expected);
    if (isMissing(value))
        return;
    if (typeOf(value) != expected)
        throw ContractError("Value does not match the declared TypeSpec");
    std::visit(
        [](const auto &payload)
        {
            using T = std::decay_t<decltype(payload)>;
            if constexpr (std::is_same_v<T, std::string>)
            {
                validateUtf8(payload);
            }
            else if constexpr (std::is_same_v<T, DataRef<Image>>)
            {
                for (const auto &image : payload.get()) validateImageFrame(image);
            }
            else if constexpr (std::is_same_v<T, DataRef<TrainingSet>>)
            {
                validateTrainingSet(payload.get());
            }
            else if constexpr (std::is_same_v<T, Array>)
            {
                if (payload.dtype() == DType::String)
                    for (const auto &text : payload.elements<std::string>()) validateUtf8(text);
            }
        },
        value);
}

bool equalValues(const PortValue& left, const PortValue& right) {
    if (left.index() != right.index()) return false;
    return std::visit(
        [&](const auto &payload)
        {
            using T           = std::decay_t<decltype(payload)>;
            const auto &other = std::get<T>(right);
            if constexpr (std::is_same_v<T, Array>)
            {
                return payload == other;
            }
            else if constexpr (std::is_same_v<T, DataRef<TrainingSet>>)
            {
                const auto &first = payload.get();
                const auto &second = other.get();
                if (&first == &second) return true;
                if (first.images.size() != second.images.size() || first.records.size() != second.records.size()) return false;
                for (const auto &[id, image] : first.images)
                {
                    const auto found = second.images.find(id);
                    if (found == second.images.end() || image.colorOrder != found->second.colorOrder) return false;
                    if (image.pixels.empty() != found->second.pixels.empty()) return false;
                    if (!image.pixels.empty()
                        && !equalValues(DataRef<Image>(Image{image.pixels}), DataRef<Image>(Image{found->second.pixels})))
                        return false;
                }
                for (const auto &[id, record] : first.records)
                {
                    const auto found = second.records.find(id);
                    if (found == second.records.end() || record.imageId != found->second.imageId
                        || record.fields.size() != found->second.fields.size()) return false;
                    for (const auto &[field, value] : record.fields)
                    {
                        const auto otherField = found->second.fields.find(field);
                        if (otherField == found->second.fields.end() || !equalValues(value, otherField->second)) return false;
                    }
                }
                return true;
            }
            else if constexpr (std::is_same_v<T, DataRef<Image>>)
            {
                const auto &images      = payload.get();
                const auto &otherImages = other.get();
                if (images.size() != otherImages.size())
                    return false;
                return std::equal(images.begin(), images.end(), otherImages.begin(),
                                  [](const cv::Mat &image, const cv::Mat &rhs)
                                  {
                                      return image.data == rhs.data && image.rows == rhs.rows && image.cols == rhs.cols
                                          && image.type() == rhs.type() && image.step[0] == rhs.step[0]
                                          && image.step[1] == rhs.step[1];
                                  });
            }
            else
            {
                return payload == other;
            }
        },
        left);
}

PortValue retainValue(const PortValue &value, Owner dependency)
{
    return std::visit(
        [&](const auto &payload) -> PortValue
        {
            using T = std::decay_t<decltype(payload)>;
            if constexpr (std::is_same_v<T, DataRef<Image>>
                          || std::is_same_v<T, DataRef<TrainingSet>> || std::is_same_v<T, Array>
                          || std::is_same_v<T, Direction> || std::is_same_v<T, CoordinateFrame2D> || isShapeValue<T>)
                return payload.retainedBy(dependency);
            else
                return payload;
        },
        value);
}

PortValue defaultValue(const TypeSpec &type)
{
    validateTypeSpec(type);
    switch (type.type)
    {
    case Type::Int:
        return std::int64_t{0};
    case Type::Float:
        return 0.0;
    case Type::Bool:
        return false;
    case Type::String:
        return std::string{};
    case Type::Image:
        return DataRef<Image>(Image{});
    case Type::Array:
        return Array::empty(*type.dtype, type.rank);
    case Type::TrainingSet:
        return DataRef<TrainingSet>(TrainingSet{});
    case Type::CoordinateFrame2D:
        return CoordinateFrame2D{};
    case Type::Direction:
        return Direction{};
#define VRT_SHAPE(name, kind, view, code, wire, label) case Type::name: return name{};
#include "visionrt/contracts/ShapeKinds.def"
#undef VRT_SHAPE
    }
    throw ContractError("未知端口类型");
}
}
