#include "visionrt/contracts/DeclarationJson.h"

#include "DeclarationNames.h"
#include "DeclarationValue.h"
#include "JsonDocument.h"

#include <algorithm>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>

namespace visionrt {
namespace {

using decljson::Kind;
using decljson::Node;

std::string context(std::string_view base, std::string_view field)
{
    return std::string(base) + "." + std::string(field);
}

const Node &object(const Node &node, std::string_view where)
{
    if (node.kind != Kind::Object)
        throw ContractError(std::string(where) + " 必须是对象");
    return node;
}

const Node &member(const Node &node, std::string_view name, std::string_view where)
{
    const auto *value = node.find(name);
    if (value == nullptr)
        throw ContractError(std::string(where) + " 缺少 " + std::string(name));
    return *value;
}

void fields(const Node &node, std::initializer_list<std::string_view> allowed, std::string_view where)
{
    for (const auto &[name, value] : node.members)
        if (std::find(allowed.begin(), allowed.end(), name) == allowed.end())
            throw ContractError(std::string(where) + " 包含未知字段：" + name);
}

std::string text(const Node &node, std::string_view name, std::string_view where, bool allowEmpty = true)
{
    const auto &value = member(node, name, where);
    if (value.kind != Kind::String)
        throw ContractError(context(where, name) + " 必须是字符串");
    validateUtf8(value.text);
    if (!allowEmpty && value.text.empty())
        throw ContractError(context(where, name) + " 不能为空");
    return value.text;
}

std::string optionalText(const Node &node, std::string_view name, std::string_view where)
{
    const auto *value = node.find(name);
    if (value == nullptr)
        return {};
    if (value->kind != Kind::String)
        throw ContractError(context(where, name) + " 必须是字符串");
    validateUtf8(value->text);
    return value->text;
}

using decljson::enumeration;

template<class Enum>
Enum optionalEnumeration(const Node *node, Enum fallback, std::string_view where)
{
    return node == nullptr ? fallback : enumeration<Enum>(*node, where);
}

// A declaration names files inside the tool's own tree; an absolute path would make the tool depend
// on one machine and is rejected at the reading edge, not repaired into a relative one.
std::filesystem::path relativePath(const std::string &text, std::string_view where)
{
    const auto path
        = std::filesystem::path(std::u8string_view(reinterpret_cast<const char8_t *>(text.data()), text.size()));
    if (path.empty() || path.is_absolute() || path.has_root_name() || path.has_root_directory())
        throw ContractError(std::string(where) + " 必须是相对路径");
    return path.lexically_normal();
}

PortId portId(const Node &node, std::string_view where);

PortRef reference(const Node &node, std::string_view where)
{
    const auto &block = object(node, where);
    fields(block, {"stage", "id"}, where);
    PortRef result;
    result.stage = enumeration<PortStage>(member(block, "stage", where), context(where, "stage"));
    result.id    = portId(member(block, "id", where), context(where, "id"));
    return result;
}

PortId portId(const Node &node, std::string_view where)
{
    const auto value = decljson::integer(node, where.data());
    if (value < 0 || value > static_cast<std::int64_t>(std::numeric_limits<PortId>::max()))
        throw ContractError(std::string(where) + " 必须是 32 位无符号整数");
    return static_cast<PortId>(value);
}

std::optional<PortValue> optionalValue(const Node &owner, std::string_view name, const TypeSpec &type,
                                       std::string_view where)
{
    const auto *value = owner.find(name);
    if (value == nullptr)
        return std::nullopt;
    try
    {
        return decljson::decodeValue(*value, type);
    }
    catch (const ContractError &error)
    {
        throw ContractError(context(where, name) + "：" + error.what());
    }
}

// Constraints and editor spelling are shared by value ports and training parameters: both name a
// typed value with the same limits and the same control. Only the port adds tab, group and provider.
void readConstraints(const Node &item, const TypeSpec &type, std::string_view where, std::optional<PortValue> &minimum,
                     std::optional<PortValue> &maximum, std::vector<PortValue> &options)
{
    const auto *constraints = item.find("constraints");
    if (constraints == nullptr)
        return;
    const auto &block = object(*constraints, std::string(where) + ".constraints");
    fields(block, {"min", "max", "options"}, std::string(where) + ".constraints");
    if (const auto value = optionalValue(block, "min", type, std::string(where) + ".constraints"))
        minimum = *value;
    if (const auto value = optionalValue(block, "max", type, std::string(where) + ".constraints"))
        maximum = *value;
    if (const auto *list = block.find("options"))
    {
        if (list->kind != Kind::Array)
            throw ContractError(std::string(where) + ".constraints.options 必须是数组");
        for (const auto &option : list->items)
        {
            try
            {
                options.push_back(decljson::decodeValue(option, type));
            }
            catch (const ContractError &error)
            {
                throw ContractError(std::string(where) + ".constraints.options：" + error.what());
            }
        }
    }
}

std::vector<PortDefinition> ports(const Node &root, PortStage stage)
{
    // The declaration spells the initialisation list "init_params"; diagnostics keep the shorter
    // stage name, so the two spellings live side by side by design.
    const auto  direction = stage == PortStage::Init   ? std::string("init_params")
                          : stage == PortStage::Inputs ? std::string("inputs")
                                                       : std::string("outputs");
    const auto *items     = root.find(direction);
    if (items == nullptr)
        return {};
    if (items->kind != Kind::Array)
        throw ContractError(direction + " 必须是数组");
    const bool                  output = stage == PortStage::Outputs;
    std::vector<PortDefinition> result;
    result.reserve(items->items.size());
    for (std::size_t index = 0; index < items->items.size(); ++index)
    {
        const auto  where = direction + "[" + std::to_string(index) + "]";
        const auto &item  = object(items->items[index], where);
        fields(
            item,
            {"id", "name_zh", "name_en", "type", "type_from", "default", "constraints", "ui", "relations", "display"},
            where);
        PortDefinition port;
        port.id     = portId(member(item, "id", where), context(where, "id"));
        port.nameZh = text(item, "name_zh", where);
        if (item.has("name_en"))
            port.nameEn = optionalText(item, "name_en", where);
        if (const auto *typeFrom = item.find("type_from"))
        {
            for (const auto *name : {"type", "default", "constraints", "ui", "relations", "display"})
                if (item.has(name))
                    throw ContractError(where + " 的 type_from 不能同时声明 " + name);
            const auto &block = object(*typeFrom, where + ".type_from");
            fields(block, {"flow", "node", "port"}, where + ".type_from");
            port.typeFrom
                = PortTypeReference{reference(member(block, "flow", where + ".type_from"), where + ".type_from.flow"),
                                    reference(member(block, "node", where + ".type_from"), where + ".type_from.node"),
                                    reference(member(block, "port", where + ".type_from"), where + ".type_from.port")};
        }
        else
        {
            port.type = decljson::decodeType(member(item, "type", where));
        }
        readConstraints(item, port.type, where, port.minimum, port.maximum, port.options);
        if (const auto *ui = item.find("ui"))
        {
            const auto &block = object(*ui, where + ".ui");
            fields(block, {"editor", "tab", "group", "step", "decimals", "options_provider"}, where + ".ui");
            port.editor
                = optionalEnumeration<EditorKind>(block.find("editor"), EditorKind::Default, where + ".ui.editor");
            port.tab
                = optionalEnumeration<ParameterTab>(block.find("tab"), ParameterTab::BasicInput, where + ".ui.tab");
            if (block.has("group"))
                port.group = optionalText(block, "group", where + ".ui");
            port.optionsProvider = optionalEnumeration<OptionsProvider>(
                block.find("options_provider"), OptionsProvider::None, where + ".ui.options_provider");
            if (const auto step = optionalValue(block, "step", port.type, where + ".ui"))
                port.step = *step;
            if (const auto *decimals = block.find("decimals"))
            {
                const auto value = decljson::integer(*decimals, "decimals");
                if (value < 0 || value > std::numeric_limits<int>::max())
                    throw ContractError(where + ".ui.decimals 必须是非负 int 范围整数");
                port.decimals = static_cast<int>(value);
            }
        }
        if (const auto *relations = item.find("relations"))
        {
            const auto &block = object(*relations, where + ".relations");
            fields(block, {"image", "alignment", "same_geometry_as"}, where + ".relations");
            if (const auto *image = block.find("image"))
                port.image = reference(*image, where + ".relations.image");
            if (const auto *alignment = block.find("alignment"))
                port.alignment = reference(*alignment, where + ".relations.alignment");
            if (const auto *same = block.find("same_geometry_as"))
                port.sameGeometryAs = reference(*same, where + ".relations.same_geometry_as");
        }
        if (const auto *display = item.find("display"))
        {
            const auto &block = object(*display, where + ".display");
            fields(block, {"color_order", "visible"}, where + ".display");
            port.colorOrder = optionalEnumeration<ColorOrder>(block.find("color_order"), ColorOrder::Automatic,
                                                              where + ".display.color_order");
            if (const auto *visible = block.find("visible"))
            {
                if (visible->kind != Kind::Bool)
                    throw ContractError(where + ".display.visible 必须是布尔值");
                port.imageVisible = visible->boolean;
            }
        }
        if (const auto value = optionalValue(item, "default", port.type, where))
            port.defaultValue = *value;
        if (output)
        {
            for (const auto *name : {"default", "constraints", "ui"})
                if (item.has(name))
                    throw ContractError(std::string(name) + " 不属于输出端口（" + where + "）");
        }
        if (isReservedPort(stage, port.id))
            throw ContractError("工具声明不得定义宿主公共端口：" + portLabel(port.id));
        result.push_back(std::move(port));
    }
    return result;
}

void readImplementation(const Node &root, ToolDeclaration &declaration)
{
    const auto &implementation = object(member(root, "implementation", "工具声明"), "implementation");
    fields(implementation, {"kind", "binding", "package", "file", "symbol", "resources"}, "implementation");
    const auto kind    = enumeration<ToolKind>(member(implementation, "kind", "implementation"), "implementation.kind");
    declaration.kind   = kind;
    const auto allowed = [kind](std::string_view name)
    {
        switch (kind)
        {
        case ToolKind::Host:
            return name == "binding";
        case ToolKind::PythonFunction:
        case ToolKind::PythonClass:
            return name == "package" || name == "file" || name == "symbol";
        case ToolKind::Cpp:
            return false;
        }
        return false;
    };
    for (const auto *name : {"binding", "package", "file", "symbol"})
        if (implementation.has(name) && !allowed(name))
            throw ContractError(std::string("implementation.") + name + " 不属于所选实现类型");
    switch (kind)
    {
    case ToolKind::Cpp:
        break;
    case ToolKind::Host:
        declaration.hostBinding = text(implementation, "binding", "implementation");
        break;
    case ToolKind::PythonFunction:
    case ToolKind::PythonClass:
        declaration.entry.package
            = relativePath(text(implementation, "package", "implementation"), "implementation.package");
        declaration.entry.file   = relativePath(text(implementation, "file", "implementation"), "implementation.file");
        declaration.entry.symbol = text(implementation, "symbol", "implementation");
        break;
    }
    if (const auto *resources = implementation.find("resources"))
    {
        if (resources->kind != Kind::Array)
            throw ContractError("implementation.resources 必须是数组");
        for (const auto &value : resources->items)
        {
            if (value.kind != Kind::String)
                throw ContractError("implementation.resources 必须是字符串数组");
            declaration.resources.push_back(relativePath(value.text, "implementation.resources"));
        }
    }
}

TrainingFieldSpec readTrainingField(const Node &node, std::string_view where)
{
    const auto &item = object(node, where);
    fields(item, {"id", "name_zh", "type", "roi", "regions"}, where);
    TrainingFieldSpec field;
    field.id     = portId(member(item, "id", where), context(where, "id"));
    field.nameZh = text(item, "name_zh", where, false);
    field.type   = decljson::decodeType(member(item, "type", where));
    if (field.type.type == Type::Image || field.type.type == Type::TrainingSet)
        throw ContractError(std::string(where) + " 只能声明标量、Array 或 ROI 字段");
    if (const auto *roi = item.find("roi"))
    {
        if (roi->kind != Kind::Bool)
            throw ContractError(context(where, "roi") + " 必须是布尔值");
        field.roi = roi->boolean;
    }
    if (const auto *regions = item.find("regions"))
    {
        if (!field.roi)
            throw ContractError(std::string(where) + ".regions 只属于画布编辑的 ROI 字段");
        const auto &block = object(*regions, context(where, "regions"));
        fields(block, {"min", "max"}, context(where, "regions"));
        if (const auto *minimum = block.find("min"))
            field.minRegions = static_cast<int>(decljson::integer(*minimum, context(where, "regions.min")));
        if (const auto *maximum = block.find("max"))
            field.maxRegions = static_cast<int>(decljson::integer(*maximum, context(where, "regions.max")));
        if (field.minRegions < 0 || field.maxRegions < 0)
            throw ContractError(context(where, "regions") + " 的上下限不能为负数");
        if (field.maxRegions != 0 && field.minRegions > field.maxRegions)
            throw ContractError(context(where, "regions") + " 的下限大于上限");
    }
    if (field.roi && field.type.type != Type::ROI)
        throw ContractError(std::string(where) + " 的画布字段必须是 ROI");
    return field;
}

void readTrainingRecords(const Node &node, TrainingRecordsSpec &records)
{
    const auto &block = object(node, "training.records");
    fields(block, {"max", "fields"}, "training.records");
    if (const auto *maximum = block.find("max"))
    {
        const auto value = decljson::integer(*maximum, "training.records.max");
        if (value < 0 || value > std::numeric_limits<int>::max())
            throw ContractError("training.records.max 必须是 32 位非负整数");
        records.maxCount = static_cast<int>(value);
    }
    const auto *list = block.find("fields");
    if (list == nullptr || list->kind != Kind::Array)
        throw ContractError("training.records.fields 必须是数组");
    for (std::size_t index = 0; index < list->items.size(); ++index)
    {
        const auto where = std::string("training.records.fields[") + std::to_string(index) + "]";
        auto       field = readTrainingField(list->items[index], where);
        if (std::any_of(records.fields.begin(), records.fields.end(),
                        [&](const TrainingFieldSpec &existing) { return existing.id == field.id; }))
            throw ContractError(where + " 重复声明了字段编号");
        records.fields.push_back(std::move(field));
    }
    if (std::count_if(records.fields.begin(), records.fields.end(),
                      [](const TrainingFieldSpec &field) { return field.roi; })
        > 1)
        throw ContractError("training.records.fields 只能有一个画布 ROI 字段");
}

std::vector<OutputMapping> readWrites(const Node &item, std::string_view where)
{
    std::vector<OutputMapping> result;
    const auto                *writes = item.find("writes");
    if (writes == nullptr)
        return result;
    if (writes->kind != Kind::Array)
        throw ContractError(std::string(where) + ".writes 必须是数组");
    for (std::size_t index = 0; index < writes->items.size(); ++index)
    {
        const auto  writeWhere = std::string(where) + ".writes[" + std::to_string(index) + "]";
        const auto &entry      = object(writes->items[index], writeWhere);
        fields(entry, {"source_port", "target"}, writeWhere);
        OutputMapping write;
        write.sourcePort = portId(member(entry, "source_port", writeWhere), writeWhere + ".source_port");
        if (write.sourcePort == 0)
            throw ContractError(writeWhere + ".source_port 必须大于 0");
        write.target = reference(member(entry, "target", writeWhere), writeWhere + ".target");
        if (std::any_of(result.begin(), result.end(),
                        [&](const OutputMapping &existing) { return existing.target == write.target; }))
            throw ContractError(writeWhere + ".target 重复写入同一端口：" + formatPortRef(write.target));
        result.push_back(std::move(write));
    }
    return result;
}

TrainingParameterSpec readTrainingParameter(const Node &node, std::string_view where)
{
    const auto &item = object(node, where);
    fields(item, {"id", "name_zh", "type", "default", "constraints", "ui"}, where);
    TrainingParameterSpec parameter;
    parameter.id = portId(member(item, "id", where), context(where, "id"));
    if (parameter.id == 0)
        throw ContractError(context(where, "id") + " 必须大于 0");
    parameter.nameZh = text(item, "name_zh", where, false);
    parameter.type   = decljson::decodeType(member(item, "type", where));
    if (parameter.type.type == Type::Image || parameter.type.type == Type::TrainingSet)
        throw ContractError(std::string(where) + " 只能声明标量、Array 或 ROI 参数");
    if (const auto value = optionalValue(item, "default", parameter.type, where))
        parameter.defaultValue = *value;
    readConstraints(item, parameter.type, where, parameter.minimum, parameter.maximum, parameter.options);
    if (const auto *ui = item.find("ui"))
    {
        const auto &block = object(*ui, context(where, "ui"));
        fields(block, {"editor", "step", "decimals"}, context(where, "ui"));
        parameter.editor
            = optionalEnumeration<EditorKind>(block.find("editor"), EditorKind::Default, context(where, "ui.editor"));
        if (const auto step = optionalValue(block, "step", parameter.type, context(where, "ui")))
            parameter.step = *step;
        if (const auto *decimals = block.find("decimals"))
        {
            const auto value = decljson::integer(*decimals, context(where, "ui.decimals"));
            if (value < 0 || value > std::numeric_limits<int>::max())
                throw ContractError(context(where, "ui.decimals") + " 必须是非负 int 范围整数");
            parameter.decimals = static_cast<int>(value);
        }
    }
    return parameter;
}

TrainingArtifactSpec readTrainingArtifact(const Node &node, std::string_view where)
{
    const auto &item = object(node, where);
    fields(item, {"id", "source_port", "format", "version", "target"}, where);
    TrainingArtifactSpec artifact;
    artifact.id     = text(item, "id", where, false);
    artifact.format = text(item, "format", where, false);
    if (artifact.id == "." || artifact.id == ".." || artifact.id.find_first_of("/\\:") != std::string::npos)
        throw ContractError(std::string(where) + ".id 不能用作文件路径");
    artifact.sourcePort = portId(member(item, "source_port", where), context(where, "source_port"));
    if (artifact.sourcePort == 0)
        throw ContractError(context(where, "source_port") + " 必须大于 0");
    if (const auto *version = item.find("version"))
    {
        const auto value = decljson::integer(*version, context(where, "version"));
        if (value <= 0 || value > std::numeric_limits<int>::max())
            throw ContractError(context(where, "version") + " 必须是正整数");
        artifact.version = static_cast<int>(value);
    }
    artifact.target = reference(member(item, "target", where), context(where, "target"));
    return artifact;
}

ToolCall readToolCall(const Node &node, std::string_view where);

void readTraining(const Node &node, ToolDeclaration &declaration)
{
    const auto &item = object(node, "training");
    fields(item, {"image", "records", "call", "parameters", "writes", "artifacts", "overlay_output", "region_input"},
           "training");
    TrainingSpec spec;
    if (const auto *region = item.find("region_input"))
    {
        spec.regionInput = portId(*region, "training.region_input");
        if (spec.regionInput == 0)
            throw ContractError("training.region_input 必须大于 0");
    }
    if (const auto *image = item.find("image"))
        spec.image = reference(*image, "training.image");
    readTrainingRecords(member(item, "records", "training"), spec.records);
    if (spec.records.fields.empty() && spec.regionInput == 0)
        throw ContractError("training.records.fields 为空时必须声明 region_input");
    if (const auto *call = item.find("call"))
        spec.call = readToolCall(*call, "training.call");
    if (const auto *parameters = item.find("parameters"))
    {
        if (parameters->kind != Kind::Array)
            throw ContractError("training.parameters 必须是数组");
        for (std::size_t index = 0; index < parameters->items.size(); ++index)
        {
            const auto where     = std::string("training.parameters[") + std::to_string(index) + "]";
            auto       parameter = readTrainingParameter(parameters->items[index], where);
            if (std::any_of(spec.parameters.begin(), spec.parameters.end(),
                            [&](const TrainingParameterSpec &existing) { return existing.id == parameter.id; }))
                throw ContractError(where + " 重复声明了参数编号");
            spec.parameters.push_back(std::move(parameter));
        }
    }
    if (const auto *overlay = item.find("overlay_output"))
    {
        spec.overlayOutput = portId(*overlay, "training.overlay_output");
        if (spec.overlayOutput == 0)
            throw ContractError("training.overlay_output 必须大于 0");
        if (spec.call.tool.empty())
            throw ContractError("training.overlay_output 需要训练器调用");
    }
    spec.writes = readWrites(item, "training");
    if (const auto *artifacts = item.find("artifacts"))
    {
        if (artifacts->kind != Kind::Array)
            throw ContractError("training.artifacts 必须是数组");
        for (std::size_t index = 0; index < artifacts->items.size(); ++index)
        {
            const auto where    = std::string("training.artifacts[") + std::to_string(index) + "]";
            auto       artifact = readTrainingArtifact(artifacts->items[index], where);
            if (std::any_of(spec.artifacts.begin(), spec.artifacts.end(),
                            [&](const TrainingArtifactSpec &existing) { return existing.id == artifact.id; }))
                throw ContractError(where + ".id 重复声明了产物：" + artifact.id);
            spec.artifacts.push_back(std::move(artifact));
        }
    }
    if (spec.call.tool.empty() || (spec.writes.empty() && spec.artifacts.empty()))
        throw ContractError("training 需要训练器调用及参数或模型产物");
    declaration.training = std::move(spec);
}

ToolCall readToolCall(const Node &node, std::string_view where)
{
    const auto &item = object(node, where);
    fields(item, {"tool", "inputs"}, where);
    ToolCall call;
    call.tool          = text(item, "tool", where, false);
    const auto *inputs = item.find("inputs");
    if (inputs == nullptr)
        return call;
    if (inputs->kind != Kind::Array)
        throw ContractError(context(where, "inputs") + " 必须是数组");
    for (std::size_t index = 0; index < inputs->items.size(); ++index)
    {
        const auto  entryWhere = context(where, "inputs") + "[" + std::to_string(index) + "]";
        const auto &entry      = object(inputs->items[index], entryWhere);
        fields(entry, {"port", "context", "source", "literal", "field_id", "parameter"}, entryWhere);
        ToolCallInput input;
        input.port = portId(member(entry, "port", entryWhere), context(entryWhere, "port"));
        if (input.port == 0)
            throw ContractError(context(entryWhere, "port") + " 必须大于 0");
        if (std::any_of(call.inputs.begin(), call.inputs.end(),
                        [&](const ToolCallInput &existing) { return existing.port == input.port; }))
            throw ContractError(context(entryWhere, "port") + " 重复声明了助手端口");
        const int sources = static_cast<int>(entry.has("context")) + static_cast<int>(entry.has("source"))
                          + static_cast<int>(entry.has("literal")) + static_cast<int>(entry.has("field_id"))
                          + static_cast<int>(entry.has("parameter"));
        if (sources != 1)
            throw ContractError(entryWhere + " 必须且只能声明 context、source、literal、field_id 或 parameter");
        if (const auto *role = entry.find("context"))
        {
            if (role->kind != Kind::String || role->text != "training_set")
                throw ContractError(entryWhere + ".context 只支持 training_set");
            input.source = TrainingSetSource{};
        }
        else if (const auto *sourceRef = entry.find("source"))
            input.source = reference(*sourceRef, context(entryWhere, "source"));
        else if (const auto *literal = entry.find("literal"))
        {
            const auto &block = object(*literal, context(entryWhere, "literal"));
            fields(block, {"type", "value"}, context(entryWhere, "literal"));
            TypedLiteral value;
            value.type = decljson::decodeType(member(block, "type", entryWhere));
            if (value.type.type != Type::Int && value.type.type != Type::Float && value.type.type != Type::Bool
                && value.type.type != Type::String)
                throw ContractError(context(entryWhere, "literal") + " 只支持标量常量");
            value.value  = decljson::decodeValue(member(block, "value", entryWhere), value.type);
            input.source = std::move(value);
        }
        else if (const auto *field = entry.find("field_id"))
        {
            const auto id = portId(*field, context(entryWhere, "field_id"));
            if (id == 0)
                throw ContractError(context(entryWhere, "field_id") + " 必须大于 0");
            input.source = TrainingFieldRef{id};
        }
        else
        {
            const auto id = portId(*entry.find("parameter"), context(entryWhere, "parameter"));
            if (id == 0)
                throw ContractError(context(entryWhere, "parameter") + " 必须大于 0");
            input.source = TrainingParameterRef{id};
        }
        call.inputs.push_back(std::move(input));
    }
    return call;
}

void writeReference(decljson::Writer &writer, const PortRef &reference)
{
    writer.StartObject();
    writer.Key("stage");
    writer.String(decljson::wireName(reference.stage));
    writer.Key("id");
    writer.Uint(reference.id);
    writer.EndObject();
}

void writeToolCall(decljson::Writer &writer, const ToolCall &call)
{
    writer.StartObject();
    writer.Key("tool");
    writer.String(call.tool.c_str(), static_cast<rapidjson::SizeType>(call.tool.size()));
    if (!call.inputs.empty())
    {
        writer.Key("inputs");
        writer.StartArray();
        for (const auto &input : call.inputs)
        {
            writer.StartObject();
            writer.Key("port");
            writer.Uint(input.port);
            if (const auto *role = std::get_if<TrainingSetSource>(&input.source))
            {
                writer.Key("context");
                writer.String("training_set");
            }
            else if (const auto *reference = std::get_if<PortRef>(&input.source))
            {
                writer.Key("source");
                writeReference(writer, *reference);
            }
            else if (const auto *literal = std::get_if<TypedLiteral>(&input.source))
            {
                writer.Key("literal");
                writer.StartObject();
                writer.Key("type");
                decljson::encodeType(writer, literal->type);
                writer.Key("value");
                decljson::encodeValue(writer, literal->value, literal->type);
                writer.EndObject();
            }
            else if (const auto *field = std::get_if<TrainingFieldRef>(&input.source))
            {
                writer.Key("field_id");
                writer.Uint(field->id);
            }
            else
            {
                writer.Key("parameter");
                writer.Uint(std::get<TrainingParameterRef>(input.source).id);
            }
            writer.EndObject();
        }
        writer.EndArray();
    }
    writer.EndObject();
}

void writeWrites(decljson::Writer &writer, const char *name, const std::vector<OutputMapping> &writes)
{
    if (writes.empty())
        return;
    writer.Key(name);
    writer.StartArray();
    for (const auto &entry : writes)
    {
        writer.StartObject();
        writer.Key("source_port");
        writer.Uint(entry.sourcePort);
        writer.Key("target");
        writeReference(writer, entry.target);
        writer.EndObject();
    }
    writer.EndArray();
}

void writeDefault(decljson::Writer &writer, const char *name, const PortValue &value, const TypeSpec &type)
{
    writer.Key(name);
    decljson::encodeValue(writer, value, type);
}

void writePorts(decljson::Writer &writer, std::string_view direction, const std::vector<PortDefinition> &ports,
                PortStage stage)
{
    const auto authored = std::count_if(
        ports.begin(), ports.end(), [stage](const PortDefinition &port) { return !isReservedPort(stage, port.id); });
    if (authored == 0)
        return;
    writer.Key(std::string(direction).c_str(), static_cast<rapidjson::SizeType>(direction.size()));
    writer.StartArray();
    for (const auto &port : ports)
    {
        if (isReservedPort(stage, port.id))
            continue;
        writer.StartObject();
        writer.Key("id");
        writer.Uint(port.id);
        writer.Key("name_zh");
        writer.String(port.nameZh.c_str(), static_cast<rapidjson::SizeType>(port.nameZh.size()));
        if (!port.nameEn.empty())
        {
            writer.Key("name_en");
            writer.String(port.nameEn.c_str(), static_cast<rapidjson::SizeType>(port.nameEn.size()));
        }
        if (port.typeFrom)
        {
            writer.Key("type_from");
            writer.StartObject();
            writer.Key("flow");
            writeReference(writer, port.typeFrom->flowParameter);
            writer.Key("node");
            writeReference(writer, port.typeFrom->nodeParameter);
            writer.Key("port");
            writeReference(writer, port.typeFrom->portParameter);
            writer.EndObject();
        }
        else
        {
            writer.Key("type");
            decljson::encodeType(writer, port.type);
            if (!isMissing(port.defaultValue) && !equalValues(port.defaultValue, defaultValue(port.type)))
                writeDefault(writer, "default", port.defaultValue, port.type);
        }
        if (port.minimum || port.maximum || !port.options.empty())
        {
            writer.Key("constraints");
            writer.StartObject();
            if (port.minimum)
                writeDefault(writer, "min", *port.minimum, port.type);
            if (port.maximum)
                writeDefault(writer, "max", *port.maximum, port.type);
            if (!port.options.empty())
            {
                writer.Key("options");
                writer.StartArray();
                for (const auto &option : port.options) decljson::encodeValue(writer, option, port.type);
                writer.EndArray();
            }
            writer.EndObject();
        }
        const bool hasUi = port.editor != EditorKind::Default
                        || (stage != PortStage::Outputs && port.tab != ParameterTab::BasicInput) || !port.group.empty()
                        || port.optionsProvider != OptionsProvider::None || port.step || port.decimals;
        if (hasUi)
        {
            writer.Key("ui");
            writer.StartObject();
            if (port.editor != EditorKind::Default)
            {
                writer.Key("editor");
                writer.String(decljson::wireName(port.editor));
            }
            if (stage != PortStage::Outputs && port.tab != ParameterTab::BasicInput)
            {
                writer.Key("tab");
                writer.String(decljson::wireName(port.tab));
            }
            if (!port.group.empty())
            {
                writer.Key("group");
                writer.String(port.group.c_str(), static_cast<rapidjson::SizeType>(port.group.size()));
            }
            if (port.optionsProvider != OptionsProvider::None)
            {
                writer.Key("options_provider");
                writer.String(decljson::wireName(port.optionsProvider));
            }
            if (port.step)
                writeDefault(writer, "step", *port.step, port.type);
            if (port.decimals)
            {
                writer.Key("decimals");
                writer.Int(*port.decimals);
            }
            writer.EndObject();
        }
        const bool hasRelations = port.image || port.alignment || port.sameGeometryAs;
        if (hasRelations)
        {
            writer.Key("relations");
            writer.StartObject();
            if (port.image)
            {
                writer.Key("image");
                writeReference(writer, *port.image);
            }
            if (port.alignment)
            {
                writer.Key("alignment");
                writeReference(writer, *port.alignment);
            }
            if (port.sameGeometryAs)
            {
                writer.Key("same_geometry_as");
                writeReference(writer, *port.sameGeometryAs);
            }
            writer.EndObject();
        }
        const bool hasDisplay = port.type.type == Type::Image || port.colorOrder != ColorOrder::Automatic;
        if (hasDisplay)
        {
            writer.Key("display");
            writer.StartObject();
            if (port.type.type == Type::Image)
            {
                writer.Key("visible");
                writer.Bool(port.imageVisible);
            }
            if (port.colorOrder != ColorOrder::Automatic)
            {
                writer.Key("color_order");
                writer.String(decljson::wireName(port.colorOrder));
            }
            writer.EndObject();
        }
        writer.EndObject();
    }
    writer.EndArray();
}

} // namespace

ToolDeclaration readToolDeclaration(std::string_view json, std::string_view source)
{
    const auto prefix = source.empty() ? std::string{} : std::string(source) + ": ";
    try
    {
        const auto root = decljson::parse(json);
        if (root.kind != Kind::Object)
            throw ContractError("工具声明必须是 JSON 对象");
        fields(root,
               {"schema_version", "name_en", "name_zh", "version", "implementation", "ui", "init_params", "inputs",
                "outputs", "node_model", "training"},
               "工具声明");
        if (decljson::integer(member(root, "schema_version", "工具声明"), "schema_version") != 2)
            throw ContractError("工具声明 schema_version 必须为 2");
        ToolDeclaration declaration;
        declaration.nameEn  = text(root, "name_en", "工具声明");
        declaration.nameZh  = text(root, "name_zh", "工具声明");
        declaration.version = text(root, "version", "工具声明");
        readImplementation(root, declaration);
        if (const auto *ui = root.find("ui"))
        {
            const auto &block = object(*ui, "ui");
            fields(block, {"visibility"}, "ui");
            declaration.visibility = optionalEnumeration<ToolVisibility>(block.find("visibility"),
                                                                         ToolVisibility::Normal, "ui.visibility");
        }
        declaration.nodeModel = optionalEnumeration<NodeModel>(root.find("node_model"), NodeModel::Fixed, "node_model");
        if (const auto *training = root.find("training"))
            readTraining(*training, declaration);
        declaration.initParams = ports(root, PortStage::Init);
        declaration.inputs     = ports(root, PortStage::Inputs);
        declaration.outputs    = ports(root, PortStage::Outputs);
        if (declaration.training)
        {
            const auto &spec  = *declaration.training;
            const auto  input = std::find_if(declaration.inputs.begin(), declaration.inputs.end(),
                                             [&](const PortDefinition &port) { return port.id == spec.image.id; });
            if (spec.image.stage != PortStage::Inputs || input == declaration.inputs.end())
                throw ContractError("training.image 必须引用一个输入端口");
            if (input->type.type != Type::Image)
                throw ContractError("training.image 必须引用 Image 输入：" + input->nameZh);
            if (spec.regionInput != 0)
            {
                const auto region
                    = std::find_if(declaration.inputs.begin(), declaration.inputs.end(),
                                   [&](const PortDefinition &port) { return port.id == spec.regionInput; });
                if (region == declaration.inputs.end() || region->type.type != Type::ROI || region->image != spec.image
                    || spec.records.roiField() != 0)
                    throw ContractError("training.region_input 必须唯一引用 training.image 上的 ROI 输入："
                                        + std::to_string(spec.regionInput));
            }
        }
        return declaration;
    }
    catch (const std::exception &error)
    {
        throw ContractError(prefix + error.what());
    }
}

ToolDeclaration readToolDeclarationFile(const std::filesystem::path &path)
{
    if (path.extension() != ".json")
        throw ContractError("工具声明仅支持 JSON：" + path.string());
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
        throw ContractError("无法读取工具声明：" + path.string());
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    const auto name = path.generic_u8string();
    return readToolDeclaration(buffer.str(),
                               std::string_view(reinterpret_cast<const char *>(name.data()), name.size()));
}

std::string writeToolDeclaration(const ToolDeclaration &declaration)
{
    rapidjson::StringBuffer buffer;
    decljson::Writer        writer(buffer);
    writer.StartObject();
    writer.Key("schema_version");
    writer.Int(2);
    writer.Key("name_en");
    writer.String(declaration.nameEn.c_str(), static_cast<rapidjson::SizeType>(declaration.nameEn.size()));
    writer.Key("name_zh");
    writer.String(declaration.nameZh.c_str(), static_cast<rapidjson::SizeType>(declaration.nameZh.size()));
    writer.Key("version");
    writer.String(declaration.version.c_str(), static_cast<rapidjson::SizeType>(declaration.version.size()));
    writer.Key("implementation");
    writer.StartObject();
    writer.Key("kind");
    writer.String(decljson::wireName(declaration.kind));
    switch (declaration.kind)
    {
    case ToolKind::Cpp:
        break;
    case ToolKind::Host:
        writer.Key("binding");
        writer.String(declaration.hostBinding.c_str(),
                      static_cast<rapidjson::SizeType>(declaration.hostBinding.size()));
        break;
    case ToolKind::PythonFunction:
    case ToolKind::PythonClass:
    {
        const auto package = declaration.entry.package.generic_u8string();
        const auto file    = declaration.entry.file.generic_u8string();
        writer.Key("package");
        writer.String(reinterpret_cast<const char *>(package.data()), static_cast<rapidjson::SizeType>(package.size()));
        writer.Key("file");
        writer.String(reinterpret_cast<const char *>(file.data()), static_cast<rapidjson::SizeType>(file.size()));
        writer.Key("symbol");
        writer.String(declaration.entry.symbol.c_str(),
                      static_cast<rapidjson::SizeType>(declaration.entry.symbol.size()));
        break;
    }
    }
    if (!declaration.resources.empty())
    {
        writer.Key("resources");
        writer.StartArray();
        for (const auto &resource : declaration.resources)
        {
            const auto text = resource.generic_u8string();
            writer.String(reinterpret_cast<const char *>(text.data()), static_cast<rapidjson::SizeType>(text.size()));
        }
        writer.EndArray();
    }
    writer.EndObject();
    if (declaration.visibility != ToolVisibility::Normal)
    {
        writer.Key("ui");
        writer.StartObject();
        writer.Key("visibility");
        writer.String(decljson::wireName(declaration.visibility));
        writer.EndObject();
    }
    writePorts(writer, "init_params", declaration.initParams, PortStage::Init);
    writePorts(writer, "inputs", declaration.inputs, PortStage::Inputs);
    writePorts(writer, "outputs", declaration.outputs, PortStage::Outputs);
    if (declaration.nodeModel != NodeModel::Fixed)
    {
        writer.Key("node_model");
        writer.String(decljson::wireName(declaration.nodeModel));
    }
    if (declaration.training)
    {
        writer.Key("training");
        writer.StartObject();

        writer.Key("image");
        writeReference(writer, declaration.training->image);
        writer.Key("records");
        writer.StartObject();
        if (declaration.training->records.maxCount != 0)
        {
            writer.Key("max");
            writer.Int(declaration.training->records.maxCount);
        }
        writer.Key("fields");
        writer.StartArray();
        for (const auto &field : declaration.training->records.fields)
        {
            writer.StartObject();
            writer.Key("id");
            writer.Uint(field.id);
            writer.Key("name_zh");
            writer.String(field.nameZh.c_str(), static_cast<rapidjson::SizeType>(field.nameZh.size()));
            writer.Key("type");
            decljson::encodeType(writer, field.type);
            if (field.roi)
            {
                writer.Key("roi");
                writer.Bool(true);
            }
            if (field.minRegions != 0 || field.maxRegions != 0)
            {
                writer.Key("regions");
                writer.StartObject();
                if (field.minRegions != 0)
                {
                    writer.Key("min");
                    writer.Int(field.minRegions);
                }
                if (field.maxRegions != 0)
                {
                    writer.Key("max");
                    writer.Int(field.maxRegions);
                }
                writer.EndObject();
            }
            writer.EndObject();
        }
        writer.EndArray();
        writer.EndObject();
        if (!declaration.training->call.tool.empty())
        {
            writer.Key("call");
            writeToolCall(writer, declaration.training->call);
        }
        if (!declaration.training->parameters.empty())
        {
            writer.Key("parameters");
            writer.StartArray();
            for (const auto &parameter : declaration.training->parameters)
            {
                writer.StartObject();
                writer.Key("id");
                writer.Uint(parameter.id);
                writer.Key("name_zh");
                writer.String(parameter.nameZh.c_str(), static_cast<rapidjson::SizeType>(parameter.nameZh.size()));
                writer.Key("type");
                decljson::encodeType(writer, parameter.type);
                if (!std::holds_alternative<Missing>(parameter.defaultValue))
                    writeDefault(writer, "default", parameter.defaultValue, parameter.type);
                if (parameter.minimum || parameter.maximum || !parameter.options.empty())
                {
                    writer.Key("constraints");
                    writer.StartObject();
                    if (parameter.minimum)
                        writeDefault(writer, "min", *parameter.minimum, parameter.type);
                    if (parameter.maximum)
                        writeDefault(writer, "max", *parameter.maximum, parameter.type);
                    if (!parameter.options.empty())
                    {
                        writer.Key("options");
                        writer.StartArray();
                        for (const auto &option : parameter.options)
                            decljson::encodeValue(writer, option, parameter.type);
                        writer.EndArray();
                    }
                    writer.EndObject();
                }
                if (parameter.editor != EditorKind::Default || parameter.step || parameter.decimals)
                {
                    writer.Key("ui");
                    writer.StartObject();
                    if (parameter.editor != EditorKind::Default)
                    {
                        writer.Key("editor");
                        writer.String(decljson::wireName(parameter.editor));
                    }
                    if (parameter.step)
                        writeDefault(writer, "step", *parameter.step, parameter.type);
                    if (parameter.decimals)
                    {
                        writer.Key("decimals");
                        writer.Int(*parameter.decimals);
                    }
                    writer.EndObject();
                }
                writer.EndObject();
            }
            writer.EndArray();
        }
        writeWrites(writer, "writes", declaration.training->writes);
        if (declaration.training->regionInput != 0)
        {
            writer.Key("region_input");
            writer.Uint(declaration.training->regionInput);
        }
        if (declaration.training->overlayOutput != 0)
        {
            writer.Key("overlay_output");
            writer.Uint(declaration.training->overlayOutput);
        }
        if (!declaration.training->artifacts.empty())
        {
            writer.Key("artifacts");
            writer.StartArray();
            for (const auto &artifact : declaration.training->artifacts)
            {
                writer.StartObject();
                writer.Key("id");
                writer.String(artifact.id.c_str(), static_cast<rapidjson::SizeType>(artifact.id.size()));
                writer.Key("source_port");
                writer.Uint(artifact.sourcePort);
                writer.Key("format");
                writer.String(artifact.format.c_str(), static_cast<rapidjson::SizeType>(artifact.format.size()));
                writer.Key("version");
                writer.Int(artifact.version);
                writer.Key("target");
                writeReference(writer, artifact.target);
                writer.EndObject();
            }
            writer.EndArray();
        }
        writer.EndObject();
    }

    writer.EndObject();
    return std::string(buffer.GetString(), buffer.GetSize()) + "\n";
}

} // namespace visionrt
