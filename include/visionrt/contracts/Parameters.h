#pragma once

// Optional author-side parameter helpers. They read and write the existing Values map; they never
// schedule work, parse JSON, resolve declarations or copy image pixels. A tool declares its own
// fields as typed members and keeps the business decisions (upstream status policy, missing input
// handling, in-place versus new output) in its own code.
//
// Nothing here throws for a bad port: every read answers with a value or a PortIssue, and every
// write answers with an issue only when the write was rejected. The numeric business code a tool
// reports for that issue is the tool's own decision.

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
        return find(id) != nullptr;
    }

    // Raw access for ports a tool inspects without committing to one payload kind. Null means the
    // port is absent or explicitly Missing.
    const PortValue *find(std::string_view id) const noexcept
    {
        const auto found = values_.find(id);
        return found == values_.end() || isMissing(found->second) ? nullptr : &found->second;
    }

    ReadResult<std::int64_t> integer(std::string_view id) const
    {
        return required<std::int64_t>(id, Type::Int);
    }

    ReadResult<double> real(std::string_view id) const
    {
        return required<double>(id, Type::Float);
    }

    ReadResult<bool> boolean(std::string_view id) const
    {
        return required<bool>(id, Type::Bool);
    }

    ReadResult<std::string> text(std::string_view id) const
    {
        return required<std::string>(id, Type::String);
    }

    ReadResult<DataRef<Image>> image(std::string_view id) const
    {
        return required<DataRef<Image>>(id, Type::Image);
    }

    ReadResult<DataRef<ROI>> roi(std::string_view id) const
    {
        return required<DataRef<ROI>>(id, Type::ROI);
    }

    ReadResult<std::optional<std::int64_t>> optionalInteger(std::string_view id) const
    {
        return optional<std::int64_t>(id, Type::Int);
    }

    ReadResult<std::optional<double>> optionalReal(std::string_view id) const
    {
        return optional<double>(id, Type::Float);
    }

    ReadResult<std::optional<bool>> optionalBoolean(std::string_view id) const
    {
        return optional<bool>(id, Type::Bool);
    }

    ReadResult<std::optional<std::string>> optionalText(std::string_view id) const
    {
        return optional<std::string>(id, Type::String);
    }

    ReadResult<std::optional<DataRef<Image>>> nullableImage(std::string_view id) const
    {
        return optional<DataRef<Image>>(id, Type::Image);
    }

    ReadResult<std::optional<DataRef<ROI>>> nullableRoi(std::string_view id) const
    {
        return optional<DataRef<ROI>>(id, Type::ROI);
    }

    // Array handles keep the caller's DataRef storage; nothing is copied or cloned.
    template<class T>
    ReadResult<DataRef<T>> array(std::string_view id) const
    {
        auto value = nullableArray<T>(id);
        if (!value)
            return value.issue();
        if (!value.value())
            return PortIssue{PortIssueKind::Missing, std::string(id), "输入数组为空"};
        return *value.value();
    }

    template<class T>
    ReadResult<std::optional<DataRef<T>>> nullableArray(std::string_view id) const
    {
        const auto *value = find(id);
        if (!value)
            return std::optional<DataRef<T>>{};
        const auto *container = std::get_if<Array>(value);
        if (!container)
            return PortIssue{PortIssueKind::TypeMismatch, std::string(id),
                             "期望数组"};
        std::optional<DataRef<T>> result;
        container->visit(
            [&](const auto &payload)
            {
                using Stored = std::decay_t<decltype(payload)>;
                if constexpr (std::is_same_v<Stored, DataRef<T>>)
                    result = payload;
            });
        if (!result)
            return PortIssue{PortIssueKind::ShapeMismatch, std::string(id),
                             "期望 " + std::string(dtypeName(detail::arrayDType<T>())) + " rank="
                                 + std::to_string(detail::ArrayRank<T>::value)};
        return result;
    }

private:
    template<class T>
    ReadResult<T> required(std::string_view id, Type expected) const
    {
        const auto *value = find(id);
        if (!value)
            return PortIssue{PortIssueKind::Missing, std::string(id), {}};
        if (const auto *payload = std::get_if<T>(value))
            return *payload;
        return PortIssue{PortIssueKind::TypeMismatch, std::string(id),
                         std::string("期望") + typeLabel(expected)};
    }

    template<class T>
    ReadResult<std::optional<T>> optional(std::string_view id, Type expected) const
    {
        // An absent or explicitly Missing port is simply "no value"; only a present value of the
        // wrong type is an author/declaration error.
        const auto *value = find(id);
        if (!value)
            return std::optional<T>{};
        if (const auto *payload = std::get_if<T>(value))
            return std::optional<T>{*payload};
        return PortIssue{PortIssueKind::TypeMismatch, std::string(id),
                         std::string("期望") + typeLabel(expected)};
    }

    const Values &values_;
};

class OutputWriter
{
public:
    PortWrite integer(std::string_view id, std::int64_t value)
    {
        return write(id, value);
    }

    PortWrite real(std::string_view id, double value)
    {
        return write(id, value);
    }

    PortWrite boolean(std::string_view id, bool value)
    {
        return write(id, value);
    }

    PortWrite text(std::string_view id, std::string value)
    {
        return write(id, std::move(value));
    }

    PortWrite image(std::string_view id, DataRef<Image> value)
    {
        return write(id, std::move(value));
    }

    PortWrite roi(std::string_view id, DataRef<ROI> value)
    {
        return write(id, std::move(value));
    }

    template<class T>
    PortWrite array(std::string_view id, DataRef<T> value)
    {
        return write(id, Array(std::move(value)));
    }

    PortWrite optionalImage(std::string_view id, const std::optional<DataRef<Image>> &value)
    {
        return value ? image(id, *value) : missing(id);
    }

    PortWrite optionalRoi(std::string_view id, const std::optional<DataRef<ROI>> &value)
    {
        return value ? roi(id, *value) : missing(id);
    }

    template<class T>
    PortWrite optionalArray(std::string_view id, const std::optional<DataRef<T>> &value)
    {
        return value ? array<T>(id, *value) : missing(id);
    }

    // A declared port with no value is written explicitly, never left for the host to invent.
    PortWrite missing(std::string_view id)
    {
        return write(id, Missing{});
    }

    // Raw access for values produced by shared kernels.
    PortWrite value(std::string_view id, PortValue value)
    {
        return write(id, std::move(value));
    }

    Values take() noexcept
    {
        return std::move(values_);
    }

    // The first rejected write, kept so a packer can report it even when it ignores return values.
    const std::optional<PortIssue> &firstIssue() const noexcept
    {
        return issue_;
    }

private:
    PortWrite write(std::string_view id, PortValue value)
    {
        // The two status ports are packed into outputs by toolResult/finish; direct business mutation is reserved.
        if (isStatusPort(id))
            return latched(PortIssue{PortIssueKind::ReservedPort, std::string(id), {}});
        auto key = std::string(id);
        if (!values_.emplace(key, std::move(value)).second)
            return latched(PortIssue{PortIssueKind::DuplicateOutput, std::move(key), {}});
        return std::nullopt;
    }

    PortWrite latched(PortIssue issue)
    {
        if (!issue_)
            issue_ = issue;
        return issue;
    }

    Values                   values_;
    std::optional<PortIssue> issue_;
};

struct IInputParameters
{
    virtual ~IInputParameters() = default;
    virtual ToolStatus parse(const Values &values) = 0;
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
    ToolStatus parse(const Values &values) final
    {
        InputReader reader(values);
        const auto  upstream = reader.optionalInteger(kUpstreamStatusPort);
        if (!upstream)
            return {kContractViolationStatus, "上游状态端口不合法", upstream.issue().describe()};
        upstreamStatus = upstream.value().value_or(0);
        return readFields(reader);
    }

protected:
    virtual ToolStatus readFields(InputReader &reader) = 0;
};

class OutputParametersBase : public IOutputParameters
{
public:
    ToolStatus status{};

    // One construction point for the declared business outputs plus the status the tool decided.
    ToolResult pack() final
    {
        OutputWriter writer;
        writeFields(writer);
        if (const auto &issue = writer.firstIssue(); issue)
            throw ContractError("工具输出端口契约不通过: " + issue->describe());
        return toolResult(writer.take(), status);
    }

protected:
    virtual void writeFields(OutputWriter &writer) = 0;
};

} // namespace visionrt
