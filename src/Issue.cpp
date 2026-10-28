#include "visionrt/contracts/Issue.h"

namespace visionrt {

std::string PortIssue::describe() const
{
    std::string result;
    switch (kind)
    {
    case PortIssueKind::Missing:
        result = "输入端口 '" + field + "' 缺失";
        break;
    case PortIssueKind::TypeMismatch:
        result = "输入端口 '" + field + "' 类型不匹配";
        break;
    case PortIssueKind::ShapeMismatch:
        result = "输入端口 '" + field + "' 形状不匹配";
        break;
    case PortIssueKind::PathUnresolved:
        result = "路径 '" + field + "' 无法解析";
        break;
    case PortIssueKind::ReservedPort:
        result = "输出端口 '" + field + "' 由宿主写入，工具不得填写";
        break;
    case PortIssueKind::DuplicateOutput:
        result = "输出端口 '" + field + "' 被重复写入";
        break;
    }
    if (!text.empty())
        result += "：" + text;
    return result;
}

} // namespace visionrt
