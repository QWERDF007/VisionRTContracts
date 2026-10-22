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

ToolStatus normalizeExecutionStatus(ToolStatus status) {
    if (!isBlankMessage(status.message)) {
        return status;
    }
    if (status.code == 0) {
        status.message = "运行成功";
    } else if (status.code == kCancelledStatus) {
        status.message = "运行已取消";
    } else {
        status.message = "运行失败（错误码：" + std::to_string(status.code) + "）";
    }
    return status;
}

} // namespace visionrt
