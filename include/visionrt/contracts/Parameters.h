#pragma once

// Optional author-side parameter helpers. They read and write the existing Values map; they never
// schedule work, parse JSON, resolve declarations or copy image pixels. A tool declares its own
// fields as typed members and keeps the business decisions (upstream status policy, missing input
// handling, in-place versus new output) in its own code.

#include "visionrt/contracts/Tool.h"

#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace visionrt {
namespace detail {

template<class T>
struct ArrayRank : std::integral_constant<int, 0>
{
};

template<class T>
struct ArrayRank<std::vector<T>> : std::integral_constant<int, ArrayRank<T>::value + 1>
{
};

template<class T>
struct ArrayScalar
{
    using type = T;
};

template<class T>
struct ArrayScalar<std::vector<T>>
{
    using type = typename ArrayScalar<T>::type;
};

template<class T>
constexpr DType arrayDType()
{
    using Scalar = typename ArrayScalar<T>::type;
    if constexpr (std::is_same_v<Scalar, std::uint8_t>)
        return DType::UInt8;
    else if constexpr (std::is_same_v<Scalar, std::int32_t>)
        return DType::Int32;
    else if constexpr (std::is_same_v<Scalar, std::int64_t>)
        return DType::Int64;
    else if constexpr (std::is_same_v<Scalar, float>)
        return DType::Float32;
    else if constexpr (std::is_same_v<Scalar, double>)
        return DType::Float64;
    else if constexpr (std::is_same_v<Scalar, bool>)
        return DType::Bool;
    else if constexpr (std::is_same_v<Scalar, std::string>)
        return DType::String;
    else
        static_assert(!sizeof(T), "不支持的数组元素类型");
}

} // namespace detail

// Central typed access. A missing key and an explicit Missing value both mean "no value"; a value
// of the wrong type is an author/declaration error and never silently becomes a default.
class InputReader
{
public:
    explicit InputReader(const Values &values) noexcept
        : values_(values)
    {
    }

    const Values &values() const noexcept
    {
        return values_;
    }

    // True only when a usable (non-Missing) value is present.
    bool present(std::string_view id) const noexcept
    {
        const auto *value = find(id);
        return value != nullptr;
    }

    std::int64_t integer(std::string_view id) const
    {
        return cast<std::int64_t>(require(id), id, Type::Int);
    }

    double real(std::string_view id) const
    {
        return cast<double>(require(id), id, Type::Float);
    }

    bool boolean(std::string_view id) const
    {
        return cast<bool>(require(id), id, Type::Bool);
    }

    std::string text(std::string_view id) const
    {
        return cast<std::string>(require(id), id, Type::String);
    }

    DataRef<Image> image(std::string_view id) const
    {
        return cast<DataRef<Image>>(require(id), id, Type::Image);
    }

    DataRef<ROI> roi(std::string_view id) const
    {
        return cast<DataRef<ROI>>(require(id), id, Type::ROI);
    }

    std::optional<std::int64_t> optionalInteger(std::string_view id) const
    {
        return optional<std::int64_t>(id, Type::Int);
    }

    std::optional<double> optionalReal(std::string_view id) const
    {
        return optional<double>(id, Type::Float);
    }

    std::optional<bool> optionalBoolean(std::string_view id) const
    {
        return optional<bool>(id, Type::Bool);
    }

    std::optional<std::string> optionalText(std::string_view id) const
    {
        return optional<std::string>(id, Type::String);
    }

    std::optional<DataRef<Image>> nullableImage(std::string_view id) const
    {
        return optional<DataRef<Image>>(id, Type::Image);
    }

    std::optional<DataRef<ROI>> nullableRoi(std::string_view id) const
    {
        return optional<DataRef<ROI>>(id, Type::ROI);
    }

    // Array handles keep the caller's DataRef storage; nothing is copied or cloned.
    template<class T>
    DataRef<T> array(std::string_view id) const
    {
        auto value = nullableArray<T>(id);
        if (!value)
            throw ContractError("输入数组为空：" + std::string(id));
        return *value;
    }

    template<class T>
    std::optional<DataRef<T>> nullableArray(std::string_view id) const
    {
        const auto *value = find(id);
        if (!value)
            return std::nullopt;
        const auto *container = std::get_if<Array>(value);
        if (!container)
            throw ContractError("输入端口类型不匹配：" + std::string(id) + "（期望 " + typeName(Type::Array)
                                + "）");
        std::optional<DataRef<T>> result;
        container->visit(
            [&](const auto &payload)
            {
                using Stored = std::decay_t<decltype(payload)>;
                if constexpr (std::is_same_v<Stored, DataRef<T>>)
                    result = payload;
            });
        if (!result)
            throw ContractError("输入数组形状不匹配：" + std::string(id) + "（期望 "
                                + std::string(dtypeName(detail::arrayDType<T>())) + " rank="
                                + std::to_string(detail::ArrayRank<T>::value) + "）");
        return result;
    }

    // Raw access for ports a tool inspects without committing to one payload kind.
    const PortValue &require(std::string_view id) const
    {
        const auto found = values_.find(id);
        if (found == values_.end())
            throw ContractError("输入端口不存在：" + std::string(id));
        if (isMissing(found->second))
            throw ContractError("输入端口为空：" + std::string(id));
        return found->second;
    }

private:
    const PortValue *find(std::string_view id) const noexcept
    {
        const auto found = values_.find(id);
        return found == values_.end() || isMissing(found->second) ? nullptr : &found->second;
    }

    template<class T>
    T cast(const PortValue &value, std::string_view id, Type expected) const
    {
        if (const auto *payload = std::get_if<T>(&value))
            return *payload;
        throw ContractError("输入端口类型不匹配：" + std::string(id) + "（期望 " + typeName(expected) + "）");
    }

    template<class T>
    std::optional<T> optional(std::string_view id, Type expected) const
    {
        const auto *value = find(id);
        if (!value)
            return std::nullopt;
        return cast<T>(*value, id, expected);
    }

    const Values &values_;
};

class OutputWriter
{
public:
    void integer(std::string_view id, std::int64_t value)
    {
        write(id, value);
    }

    void real(std::string_view id, double value)
    {
        write(id, value);
    }

    void boolean(std::string_view id, bool value)
    {
        write(id, value);
    }

    void text(std::string_view id, std::string value)
    {
        write(id, std::move(value));
    }

    void image(std::string_view id, DataRef<Image> value)
    {
        write(id, std::move(value));
    }

    void roi(std::string_view id, DataRef<ROI> value)
    {
        write(id, std::move(value));
    }

    template<class T>
    void array(std::string_view id, DataRef<T> value)
    {
        write(id, Array(std::move(value)));
    }

    void optionalImage(std::string_view id, const std::optional<DataRef<Image>> &value)
    {
        value ? image(id, *value) : missing(id);
    }

    void optionalRoi(std::string_view id, const std::optional<DataRef<ROI>> &value)
    {
        value ? roi(id, *value) : missing(id);
    }

    template<class T>
    void optionalArray(std::string_view id, const std::optional<DataRef<T>> &value)
    {
        value ? array<T>(id, *value) : missing(id);
    }

    // A declared port with no value is written explicitly, never left for the host to invent.
    void missing(std::string_view id)
    {
        write(id, Missing{});
    }

    // Raw access for values produced by shared kernels.
    void value(std::string_view id, PortValue value)
    {
        write(id, std::move(value));
    }

    Values take() noexcept
    {
        return std::move(values_);
    }

private:
    void write(std::string_view id, PortValue value)
    {
        // The two status ports belong to the base class only: one spelling of each key.
        if (id == kStatusCodePort || id == kStatusMessagePort)
            throw ContractError("状态端口由参数基类写入：" + std::string(id));
        auto key = std::string(id);
        if (!values_.emplace(key, std::move(value)).second)
            throw ContractError("输出端口重复写入：" + key);
    }

    Values values_;
};

struct IInputParameters
{
    virtual ~IInputParameters() = default;
    virtual void parse(const Values &values) = 0;
};

struct IOutputParameters
{
    virtual ~IOutputParameters() = default;
    virtual ToolResult pack() = 0;
};

class InputParametersBase : public IInputParameters
{
public:
    std::int64_t upstreamStatus = 0;

    // The upstream status is read, never acted on: each tool decides whether a non-zero status
    // stops its own algorithm, and fields stay available either way.
    void parse(const Values &values) final
    {
        InputReader reader(values);
        upstreamStatus = reader.optionalInteger(kUpstreamStatusPort).value_or(0);
        readFields(reader);
    }

protected:
    virtual void readFields(InputReader &reader) = 0;
};

class OutputParametersBase : public IOutputParameters
{
public:
    ToolStatus status{};

    // One construction point for the declared business outputs plus the two status keys the tool
    // itself decided.
    ToolResult pack() final
    {
        OutputWriter writer;
        writeFields(writer);
        return toolResult(writer.take(), status);
    }

protected:
    virtual void writeFields(OutputWriter &writer) = 0;
};

} // namespace visionrt
