#pragma once

#include "visionrt/contracts/Export.h"
#include "visionrt/contracts/Issue.h"
#include "visionrt/contracts/Value.h"

#include <cstdint>
#include <map>
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

// A tool receives only its declared values. A string that names a file is resolved by the tool that
// reads or writes it, against the directory of the running product executable; the host neither
// rewrites it nor hands the tool any directory, project, or service object.
class IToolInstance {
public:
    virtual ~IToolInstance() = default;
    virtual ToolStatus initialize(const Values& initParams) = 0;
    virtual ToolResult execute(const Values& inputs) = 0;
    virtual ToolStatus release() = 0;
};

} // namespace visionrt
