#pragma once

#include "visionrt/contracts/Value.h"
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <string_view>

namespace visionrt {

using Values = std::map<std::string, PortValue, std::less<>>;

// Host-owned cancellation result; tool authors never return it as a business code.
inline constexpr std::int64_t kCancelledStatus = -5;

struct ToolStatus {
    std::int64_t code = 0;
    std::string message;
    explicit operator bool() const noexcept { return code == 0; }
};

VT_EXPORT bool isBlankMessage(std::string_view message) noexcept;

// A successful execution status; a blank message becomes 运行成功.
VT_EXPORT ToolStatus executionSuccess(std::string message = "运行成功");

// Applies the shared execution-status rule to a final execute result.
VT_EXPORT ToolStatus normalizeExecutionStatus(ToolStatus status);

struct ToolResult {
    ToolStatus status;
    Values outputs;
};

class PathResolver {
public:
    PathResolver() = default;
    explicit PathResolver(std::filesystem::path projectDirectory)
        : directory_(std::move(projectDirectory)) {}

    std::filesystem::path resolve(const std::string& utf8) const {
        if (utf8.empty()) return {};
        const auto path = std::filesystem::path(std::u8string_view(
            reinterpret_cast<const char8_t*>(utf8.data()), utf8.size()));
        if (path.is_absolute()) return path.lexically_normal();
        if (directory_.empty())
            throw std::runtime_error("相对资源路径需要先保存工程");
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
