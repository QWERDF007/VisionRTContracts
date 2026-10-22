#include "visionrt/contracts/Value.h"

#include <algorithm>
#include <limits>
#include <type_traits>

namespace visionrt {

namespace {

template<class T>
void validateStrings(const T &value)
{
    if constexpr (std::is_same_v<T, std::string>)
    {
        validateUtf8(value);
    }
    else
    {
        for (const auto &child : value) validateStrings(child);
    }
}

void validateImage(const cv::Mat& image) {
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

}

bool isMissing(const PortValue &value) noexcept
{
    return std::holds_alternative<Missing>(value);
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
            else if constexpr (std::is_same_v<T, DataRef<ROI>>)
                return TypeSpec{Type::ROI};
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
                for (const auto &image : payload.get()) validateImage(image);
            }
            else if constexpr (std::is_same_v<T, DataRef<ROI>>)
            {
                for (const auto &group : payload.get())
                    for (const auto &roi : group) validateRoi(roi);
            }
            else if constexpr (std::is_same_v<T, Array>)
            {
                payload.visit(
                    [](const auto &data)
                    {
                        using Payload = std::remove_reference_t<decltype(data.get())>;
                        if constexpr (std::is_same_v<typename detail::ArrayTraits<Payload>::scalar_type, std::string>)
                            validateStrings(data.get());
                    });
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
                if (payload.type() != other.type())
                    return false;
                return payload.visit(
                    [&](const auto &data)
                    {
                        using Payload = std::remove_reference_t<decltype(data.get())>;
                        return data.get() == other.template get<Payload>();
                    });
            }
            else if constexpr (std::is_same_v<T, DataRef<ROI>>)
            {
                return payload.get() == other.get();
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
            if constexpr (std::is_same_v<T, DataRef<Image>> || std::is_same_v<T, DataRef<ROI>>
                          || std::is_same_v<T, Array>)
                return payload.retainedBy(dependency);
            else
                return payload;
        },
        value);
}
}
