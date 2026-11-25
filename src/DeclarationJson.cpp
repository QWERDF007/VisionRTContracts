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
                     std::optional<PortValue> &maximum, std::vector<PortOption> &options)
{
    const auto *constraints = item.find("constraints");
    if (constraints == nullptr)
        return;
    const auto &block = object(*constraints, std::string(where) + ".constraints");
    fields(block, {"min", "max", "options", "regions"}, std::string(where) + ".constraints");
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
                if (option.kind == Kind::Object && option.find("value"))
                {
                    fields(option, {"value", "label"}, std::string(where) + ".constraints.options");
                    options.emplace_back(decljson::decodeValue(*option.find("value"), type), optionalText(option, "label", where));
                }
                else options.emplace_back(decljson::decodeValue(option, type));
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
                          : stage == PortStage::Train ? std::string("train_params")
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
            {"id", "name_zh", "name_en", "type", "type_from", "default", "constraints", "ui", "relations", "display", "produced_by"},
            where);
        PortDefinition port;
        port.id     = portId(member(item, "id", where), context(where, "id"));
        port.nameZh = text(item, "name_zh", where);
        if (item.has("produced_by") && !output)
            throw ContractError(where + ".produced_by 仅属于输出");
        port.producedBy = optionalEnumeration<ProducedBy>(item.find("produced_by"), ProducedBy::Both, where + ".produced_by");
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
        if (const auto *constraints = item.find("constraints"))
            if (const auto *regions = constraints->find("regions"))
            {
                fields(object(*regions, where + ".constraints.regions"), {"min", "max"}, where);
                for (const auto &[name, target] : {std::pair{"min", &port.minRegions}, std::pair{"max", &port.maxRegions}})
                    if (const auto *bound = regions->find(name))
                    {
                        const auto number = decljson::integer(*bound, where);
                        if (number < 0 || number > std::numeric_limits<int>::max()) throw ContractError(where + " 区域数量超出范围");
                        *target = static_cast<int>(number);
                    }
            }
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

void readTraining(const Node &node, ToolDeclaration &declaration)
{
    const auto &item = object(node, "training");
    fields(item, {"datasets", "artifacts"}, "training");
    TrainingSpec spec;
    if (const auto *datasets = item.find("datasets"))
    {
        if (datasets->kind != Kind::Array) throw ContractError("training.datasets 必须是数组");
        for (const auto &entry : datasets->items)
        {
            fields(object(entry, "training.datasets"), {"parameter", "image", "max_records", "fields"}, "training.datasets");
            TrainingDatasetSpec dataset;
            dataset.parameter = portId(member(entry, "parameter", "training.datasets"), "training.datasets.parameter");
            if (const auto *image = entry.find("image")) dataset.image = reference(*image, "training.datasets.image");
            if (const auto *maximum = entry.find("max_records"))
            {
                const auto count = decljson::integer(*maximum, "training.datasets.max_records");
                if (count < 0 || count > std::numeric_limits<int>::max()) throw ContractError("训练记录上限超出范围");
                dataset.maxRecords = static_cast<int>(count);
            }
            if (const auto *recordFields = entry.find("fields"))
            {
                Node wrapper;
                wrapper.kind = Kind::Object;
                wrapper.members.emplace_back("train_params", *recordFields);
                dataset.fields = ports(wrapper, PortStage::Train);
            }
            spec.datasets.push_back(std::move(dataset));
        }
    }
    if (const auto *artifacts = item.find("artifacts"))
    {
        if (artifacts->kind != Kind::Array)
            throw ContractError("training.artifacts 必须是数组");
        for (const auto &artifact : artifacts->items)
            spec.artifacts.push_back(readTrainingArtifact(artifact, "training.artifacts"));
    }
    declaration.training = std::move(spec);
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
        if (port.producedBy != ProducedBy::Both)
        {
            writer.Key("produced_by");
            writer.String(decljson::wireName(port.producedBy));
        }
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
        if (port.minimum || port.maximum || !port.options.empty() || port.minRegions || port.maxRegions)
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
                for (const auto &option : port.options)
                {
                    if (option.label.empty()) decljson::encodeValue(writer, option.value, port.type);
                    else
                    {
                        writer.StartObject();
                        writeDefault(writer, "value", option.value, port.type);
                        writer.Key("label"); writer.String(option.label.c_str());
                        writer.EndObject();
                    }
                }
                writer.EndArray();
            }
            if (port.minRegions || port.maxRegions)
            {
                writer.Key("regions"); writer.StartObject();
                writer.Key("min"); writer.Int(port.minRegions);
                writer.Key("max"); writer.Int(port.maxRegions);
                writer.EndObject();
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
                "outputs", "train_params", "node_model", "training"},
               "工具声明");
        if (decljson::integer(member(root, "schema_version", "工具声明"), "schema_version") != kToolDeclarationSchemaVersion)
            throw ContractError("工具声明 schema_version 不受支持");
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
        declaration.trainParams = ports(root, PortStage::Train);
        declaration.initParams = ports(root, PortStage::Init);
        declaration.inputs     = ports(root, PortStage::Inputs);
        declaration.outputs    = ports(root, PortStage::Outputs);
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
    writer.Int(kToolDeclarationSchemaVersion);
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
    writePorts(writer, "train_params", declaration.trainParams, PortStage::Train);
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
        const auto &spec = *declaration.training;
        writer.Key("training"); writer.StartObject();
        if (!spec.datasets.empty())
        {
            writer.Key("datasets"); writer.StartArray();
            for (const auto &dataset : spec.datasets)
            {
                writer.StartObject();
                writer.Key("parameter"); writer.Uint(dataset.parameter);
                if (dataset.image) { writer.Key("image"); writeReference(writer, *dataset.image); }
                if (dataset.maxRecords) { writer.Key("max_records"); writer.Int(dataset.maxRecords); }
                writePorts(writer, "fields", dataset.fields, PortStage::Train);
                writer.EndObject();
            }
            writer.EndArray();
        }
        writer.Key("artifacts"); writer.StartArray();
        for (const auto &artifact : spec.artifacts)
        {
            writer.StartObject();
            writer.Key("id"); writer.String(artifact.id.c_str());
            writer.Key("source_port"); writer.Uint(artifact.sourcePort);
            writer.Key("format"); writer.String(artifact.format.c_str());
            writer.Key("version"); writer.Int(artifact.version);
            writer.Key("target"); writeReference(writer, artifact.target);
            writer.EndObject();
        }
        writer.EndArray(); writer.EndObject();
    }
    writer.EndObject();
    return std::string(buffer.GetString(), buffer.GetSize()) + "\n";
}

} // namespace visionrt
