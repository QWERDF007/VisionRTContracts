#include "visionrt/contracts/Tool.h"

#include <algorithm>
#include <string>

namespace visionrt {

bool isBlankMessage(std::string_view message) noexcept {
    return std::all_of(message.begin(), message.end(), [](char character) {
        return character == ' ' || character == '\t' || character == '\r' || character == '\n'
            || character == '\v' || character == '\f';
    });
}

ToolStatus executionSuccess(std::string message) {
    if (isBlankMessage(message)) {
        message = "运行成功";
    }
    return {0, std::move(message)};
}

ToolResult toolResult(Values outputs, ToolStatus status) {
    outputs[std::string(kStatusCodePort)] = status.code;
    outputs[std::string(kStatusMessagePort)] = std::move(status.message);
    return ToolResult{std::move(outputs), std::move(status.detail)};
}

} // namespace visionrt
