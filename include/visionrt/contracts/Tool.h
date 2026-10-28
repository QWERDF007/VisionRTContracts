#pragma once

#include "visionrt/contracts/Export.h"
#include "visionrt/contracts/Issue.h"
#include "visionrt/contracts/Value.h"

#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <string_view>

namespace visionrt {

using Values = std::map<std::string, PortValue, std::less<>>;

// Host-owned cancellation result; tool authors never return it as a business code.
inline constexpr std::int64_t kCancelledStatus = -5;

// Reserved negative code for a tool that broke the wire contract itself (a reserved port in its
// output set, a duplicate write, a malformed lifecycle answer). It is not a business outcome.
inline constexpr std::int64_t kContractViolationStatus = -1;

// Canonical ports every contract carries. They belong to the host: a node materialises them from
// the tool's ToolStatus after execute() returned, so a tool never writes them itself. They stay
// ordinary declared ports for wiring, readback and the existing output view.
inline constexpr std::string_view kUpstreamStatusPort = "upstream_status";
inline constexpr std::string_view kStatusCodePort     = "__status_code";
inline constexpr std::string_view kStatusMessagePort  = "__status_message";

inline bool isStatusPort(std::string_view id) noexcept
{
    return id == kStatusCodePort || id == kStatusMessagePort;
}

struct ToolStatus {
    std::int64_t code = 0;
    std::string message;
    std::string detail;
    explicit operator bool() const noexcept { return code == 0; }
};

VT_EXPORT bool isBlankMessage(std::string_view message) noexcept;

// A successful lifecycle status; a blank message becomes 运行成功.
VT_EXPORT ToolStatus executionSuccess(std::string message = "运行成功");

// The single execution result. `outputs` holds the declared business ports only; `status` is the
// tool's whole answer about the execution and the single source of truth for the two status ports
// the host publishes. Framework failures never reach a tool result: they are reported as
// NodeDiagnostic and NodeDisposition.
struct ToolResult {
    Values     outputs;
    ToolStatus status;
};

VT_EXPORT ToolResult toolResult(Values outputs, ToolStatus status = {});

// Turns a relative resource path into an absolute one without throwing: a relative path needs a
// saved project, which is an ordinary, reportable condition rather than an exception.
class PathResolver {
public:
    PathResolver() = default;
    explicit PathResolver(std::filesystem::path projectDirectory)
        : directory_(std::move(projectDirectory)) {}

    ReadResult<std::filesystem::path> tryResolve(std::string_view utf8) const {
        if (utf8.empty()) return std::filesystem::path{};
        const auto path = std::filesystem::path(std::u8string_view(
            reinterpret_cast<const char8_t*>(utf8.data()), utf8.size()));
        if (path.is_absolute()) return path.lexically_normal();
        if (directory_.empty())
            return PortIssue{PortIssueKind::PathUnresolved, std::string(utf8), "相对资源路径需要先保存工程"};
        return (directory_ / path).lexically_normal();
    }
    const std::filesystem::path& projectDirectory() const noexcept { return directory_; }

private:
    std::filesystem::path directory_;
};

struct ToolServices {
    PathResolver paths;
};

class IToolInstance {
public:
    virtual ~IToolInstance() = default;
    virtual ToolStatus initialize(const Values& initParams) = 0;
    virtual ToolResult execute(const Values& inputs) = 0;
    virtual ToolStatus release() = 0;
};

} // namespace visionrt
