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

// A port is identified by the tool (or node) plus its direction plus this number. Numbers are
// stable for the life of a published declaration: reordering rows, renaming a port or deleting a
// neighbour never renumbers an existing port. 2^32 is the exhaustion sentinel of a declaration's
// high-water counter, never a port number itself.
using PortId = std::uint32_t;
inline constexpr std::uint64_t kPortIdExhausted = std::uint64_t{1} << 32;

using Values = std::map<PortId, PortValue>;

// Host-owned cancellation result; tool authors never return it as a business code.
inline constexpr std::int64_t kCancelledStatus = -5;

// Reserved negative code for a tool that broke the wire contract itself.
inline constexpr std::int64_t kContractViolationStatus = -1;

// Canonical ports every contract carries. Input 0 is the upstream status injected for Fixed tools;
// outputs 0 and 1 are the status code and message the tool packs through toolResult. Business
// numbers start above them, so the same number may appear once per direction.
inline constexpr PortId kUpstreamStatusPort = 0;
inline constexpr PortId kStatusCodePort     = 0;
inline constexpr PortId kStatusMessagePort  = 1;

// Outputs 0 and 1 belong to the status envelope; direct business writes there are rejected.
inline bool isStatusPort(PortId id) noexcept
{
    return id == kStatusCodePort || id == kStatusMessagePort;
}

// A port's readable label for messages and issues ("端口 2"). Identity stays the number; this exists
// only so diagnostics stay legible.
VT_EXPORT std::string portLabel(PortId id);

struct ToolStatus {
    std::int64_t code = 0;
    std::string message;
    std::string detail;
    explicit operator bool() const noexcept { return code == 0; }
};

VT_EXPORT bool isBlankMessage(std::string_view message) noexcept;

// A successful lifecycle status; a blank message becomes 运行成功.
VT_EXPORT ToolStatus executionSuccess(std::string message = "运行成功");

// The single execution result. `outputs` holds the declared business ports plus __status_code and
// __status_message; `detail` carries technical detail for diagnostics/logs and stays separate.
struct ToolResult {
    Values      outputs;
    std::string detail;
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
