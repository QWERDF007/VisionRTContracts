#include "visionrt/contracts/Declaration.h"

namespace visionrt {

bool PortDefinition::operator==(const PortDefinition &other) const
{
    const auto optionalValueEqual = [](const auto &left, const auto &right)
    { return left.has_value() == right.has_value() && (!left || equalValues(*left, *right)); };
    return id == other.id && nameZh == other.nameZh && type == other.type && nameEn == other.nameEn
        && equalValues(defaultValue, other.defaultValue) && editor == other.editor && tab == other.tab && group == other.group
        && optionalValueEqual(step, other.step) && decimals == other.decimals && optionsProvider == other.optionsProvider
        && optionalValueEqual(minimum, other.minimum) && optionalValueEqual(maximum, other.maximum) && options == other.options
        && image == other.image && alignment == other.alignment && sameGeometryAs == other.sameGeometryAs
        && colorOrder == other.colorOrder && imageVisible == other.imageVisible && typeFrom == other.typeFrom
        && producedBy == other.producedBy && minRegions == other.minRegions && maxRegions == other.maxRegions;
}

const char *stageName(PortStage stage) noexcept
{
    switch (stage)
    {
    case PortStage::Train:
        return "train_params";
    case PortStage::Init:
        return "init";
    case PortStage::Inputs:
        return "inputs";
    case PortStage::Outputs:
        return "outputs";
    }
    return "inputs";
}

std::string formatPortRef(const PortRef &reference)
{
    return std::string(stageName(reference.stage)) + "." + std::to_string(reference.id);
}

bool isReservedPort(PortStage stage, PortId id) noexcept
{
    if (stage == PortStage::Inputs)
        return id == kUpstreamStatusPort;
    if (stage == PortStage::Outputs)
        return id < 2;
    return false;
}

PortDefinition hostPort(PortStage stage, PortId id)
{
    PortDefinition port;
    port.id = id;
    if (stage == PortStage::Inputs && id == kUpstreamStatusPort)
    {
        port.nameZh       = "上层工具状态";
        port.type         = TypeSpec{Type::Int};
        port.defaultValue = PortValue{std::int64_t{0}};
        port.tab          = ParameterTab::BasicInput;
        return port;
    }
    if (stage == PortStage::Outputs && id == kStatusCodePort)
    {
        port.nameZh = "状态码";
        port.type   = TypeSpec{Type::Int};
        return port;
    }
    if (stage == PortStage::Outputs && id == kStatusMessagePort)
    {
        port.nameZh = "状态消息";
        port.type   = TypeSpec{Type::String};
        return port;
    }
    throw ContractError("没有宿主端口：" + std::string(stageName(stage)) + " " + portLabel(id));
}

bool isInjectedInput(NodeModel model, PortId id) noexcept
{
    return model == NodeModel::Fixed && id == kUpstreamStatusPort;
}

bool isDerivedInitParam(NodeModel model, PortId id) noexcept
{
    if (model == NodeModel::Expression)
        return id == kVariablesInitPort || id == kProgramInitPort;
    if (model == NodeModel::Condition)
        return id == kVariablesInitPort || id == kProgramInitPort || id == kConditionModePort;
    return false;
}

} // namespace visionrt
