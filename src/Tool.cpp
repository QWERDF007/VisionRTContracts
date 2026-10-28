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
    // Message and detail stay separate all the way to the host: the tool describes what happened,
    // the detail stays technical, and neither is folded into the other.
    return ToolResult{std::move(outputs), std::move(status)};
}

} // namespace visionrt
