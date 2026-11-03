#pragma once

// Optional author-side parameter helpers. They read and write the existing Values map; they never
// schedule work, parse JSON, resolve declarations or copy image pixels. A tool declares its own
// fields as typed members and keeps the business decisions (upstream status policy, missing input
// handling, in-place versus new output) in its own code.
//
// Ports are addressed by the numeric id its declaration publishes. A tool keeps its own
// constexpr/enum names for those numbers; the reader never sees a spelling and there is no
// name-to-number fallback.
//
// Nothing here throws for a bad port: every read answers with a value or a PortIssue, and every
// write answers with an issue only when the write was rejected. The numeric business code a tool
// reports for that issue is the tool's own decision.

#include "visionrt/contracts/Tool.h"

#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace visionrt {

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
    bool present(PortId id) const noexcept
    {
        return find(id) != nullptr;
    }

    // Raw access for ports a tool inspects without committing to one payload kind. Null means the
    // port is absent or explicitly Missing.
    const PortValue *find(PortId id) const noexcept
    {
        const auto found = values_.find(id);
        return found == values_.end() || isMissing(found->second) ? nullptr : &found->second;
    }

    ReadResult<std::int64_t> integer(PortId id) const
    {
        return required<std::int64_t>(id, Type::Int);
    }

    ReadResult<double> real(PortId id) const
    {
        return required<double>(id, Type::Float);
    }

    ReadResult<bool> boolean(PortId id) const
    {
        return required<bool>(id, Type::Bool);
    }

    ReadResult<std::string> text(PortId id) const
    {
        return required<std::string>(id, Type::String);
    }

    ReadResult<DataRef<Image>> image(PortId id) const
    {
        return required<DataRef<Image>>(id, Type::Image);
    }

    ReadResult<DataRef<ROI>> roi(PortId id) const
    {
        return required<DataRef<ROI>>(id, Type::ROI);
    }

    ReadResult<std::optional<std::int64_t>> optionalInteger(PortId id) const
    {
        return optional<std::int64_t>(id, Type::Int);
    }

    ReadResult<std::optional<double>> optionalReal(PortId id) const
    {
        return optional<double>(id, Type::Float);
    }

    ReadResult<std::optional<bool>> optionalBoolean(PortId id) const
    {
        return optional<bool>(id, Type::Bool);
    }

    ReadResult<std::optional<std::string>> optionalText(PortId id) const
    {
        return optional<std::string>(id, Type::String);
    }

    ReadResult<std::optional<DataRef<Image>>> nullableImage(PortId id) const
    {
        return optional<DataRef<Image>>(id, Type::Image);
    }

    ReadResult<std::optional<DataRef<ROI>>> nullableRoi(PortId id) const
    {
        return optional<DataRef<ROI>>(id, Type::ROI);
    }

    // Array handles keep the caller's shared buffer; nothing is copied or cloned.
    ReadResult<Array> array(PortId id) const
    {
        auto value = nullableArray(id);
        if (!value)
            return value.issue();
        if (!value.value())
            return PortIssue{PortIssueKind::Missing, portLabel(id), "输入数组为空"};
        return *value.value();
    }

    ReadResult<std::optional<Array>> nullableArray(PortId id) const
    {
        const auto *value = find(id);
        if (!value)
            return std::optional<Array>{};
        const auto *container = std::get_if<Array>(value);
        if (!container)
            return PortIssue{PortIssueKind::TypeMismatch, portLabel(id), "期望数组"};
        return std::optional<Array>{*container};
    }

private:
    template<class T>
    ReadResult<T> required(PortId id, Type expected) const
    {
        const auto *value = find(id);
        if (!value)
            return PortIssue{PortIssueKind::Missing, portLabel(id), {}};
        if (const auto *payload = std::get_if<T>(value))
            return *payload;
        return PortIssue{PortIssueKind::TypeMismatch, portLabel(id),
                         std::string("期望") + typeLabel(expected)};
    }

    template<class T>
    ReadResult<std::optional<T>> optional(PortId id, Type expected) const
    {
        // An absent or explicitly Missing port is simply "no value"; only a present value of the
        // wrong type is an author/declaration error.
        const auto *value = find(id);
        if (!value)
            return std::optional<T>{};
        if (const auto *payload = std::get_if<T>(value))
            return std::optional<T>{*payload};
        return PortIssue{PortIssueKind::TypeMismatch, portLabel(id),
                         std::string("期望") + typeLabel(expected)};
    }

    const Values &values_;
};

class OutputWriter
{
public:
    PortWrite integer(PortId id, std::int64_t value)
    {
        return write(id, value);
    }

    PortWrite real(PortId id, double value)
    {
        return write(id, value);
    }

    PortWrite boolean(PortId id, bool value)
    {
        return write(id, value);
    }

    PortWrite text(PortId id, std::string value)
    {
        return write(id, std::move(value));
    }

    PortWrite image(PortId id, DataRef<Image> value)
    {
        return write(id, std::move(value));
    }

    PortWrite roi(PortId id, DataRef<ROI> value)
    {
        return write(id, std::move(value));
    }

    PortWrite array(PortId id, Array value)
    {
        return write(id, std::move(value));
    }

    PortWrite optionalImage(PortId id, const std::optional<DataRef<Image>> &value)
    {
        return value ? image(id, *value) : missing(id);
    }

    PortWrite optionalRoi(PortId id, const std::optional<DataRef<ROI>> &value)
    {
        return value ? roi(id, *value) : missing(id);
    }

    PortWrite optionalArray(PortId id, const std::optional<Array> &value)
    {
        return value ? array(id, *value) : missing(id);
    }

    // A declared port with no value is written explicitly, never left for the host to invent.
    PortWrite missing(PortId id)
    {
        return write(id, Missing{});
    }

    // Raw access for values produced by shared kernels.
    PortWrite value(PortId id, PortValue value)
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
    PortWrite write(PortId id, PortValue value)
    {
        // The two status ports are packed into outputs by toolResult/finish; direct business mutation is prohibited and rejected as ReservedPort.
        if (isStatusPort(id))
            return latched(PortIssue{PortIssueKind::ReservedPort, portLabel(id), {}});
        if (!values_.emplace(id, std::move(value)).second)
            return latched(PortIssue{PortIssueKind::DuplicateOutput, portLabel(id), {}});
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
