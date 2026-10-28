#pragma once

// Mechanical validation results for the port helpers. The SDK classifies only what it can check
// itself (presence, type, shape, reserved key); the business meaning of a failed read — and the
// numeric code a tool reports — stays in that tool's own error catalog.

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace visionrt {

enum class PortIssueKind
{
    Missing,
    TypeMismatch,
    ShapeMismatch,
    PathUnresolved,
    ReservedPort,
    DuplicateOutput
};

struct PortIssue
{
    PortIssueKind kind = PortIssueKind::Missing;
    std::string   field;
    std::string   text;

    // One sentence a tool can put in a status message without repeating the field twice.
    std::string describe() const;
};

// A read either produced the value or explained why it could not. There is no third state and no
// default value: a caller that wants the value has to look at ok() first.
template<class T>
class ReadResult
{
public:
    ReadResult(T value)
        : storage_(std::move(value))
    {
    }

    ReadResult(PortIssue issue)
        : storage_(std::move(issue))
    {
    }

    bool ok() const noexcept
    {
        return std::holds_alternative<T>(storage_);
    }

    explicit operator bool() const noexcept
    {
        return ok();
    }

    // Precondition: ok(). Reading the value of a failed result is a programming error.
    const T &value() const
    {
        return std::get<T>(storage_);
    }

    // Precondition: ok(). Moves the value out of a result the caller owns.
    T &&take()
    {
        return std::move(std::get<T>(storage_));
    }

    // Precondition: !ok().
    const PortIssue &issue() const
    {
        return std::get<PortIssue>(storage_);
    }

private:
    std::variant<T, PortIssue> storage_;
};

// The result of a write: empty means the port was accepted.
using PortWrite = std::optional<PortIssue>;

} // namespace visionrt
