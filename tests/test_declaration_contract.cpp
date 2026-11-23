#include "visionrt/contracts/DeclarationJson.h"

#include <cstdlib>
#include <iostream>
#include <string>

using namespace visionrt;

namespace {

void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

template<class Callable>
void rejects(Callable callable, const std::string &needle)
{
    try
    {
        callable();
    }
    catch (const ContractError &error)
    {
        if (std::string(error.what()).find(needle) == std::string::npos)
        {
            std::cerr << "diagnostic does not name '" << needle << "': " << error.what() << '\n';
            std::exit(1);
        }
        return;
    }
    std::cerr << "expected rejection naming '" << needle << "'\n";
    std::exit(1);
}

const char *kOrdinaryTool = R"({
  "schema_version": 2,
  "name_en": "test.ordinary",
  "name_zh": "普通工具",
  "version": "1.0.0",
  "implementation": { "kind": "cpp" },
  "ui": { "visibility": "internal" },
  "node_model": "select",
  "init_params": [
    { "id": 0, "name_zh": "模型路径", "type": "string" }
  ],
  "inputs": [
    { "id": 1, "name_zh": "阈值", "type": "int",
      "default": 9007199254740993,
      "constraints": { "min": 0, "max": 1000, "options": [1, 2] },
      "ui": { "editor": "number", "tab": "detection", "group": "检测", "step": 1, "decimals": 2 } },
    { "id": 2, "name_zh": "输入图像", "type": "image",
      "display": { "visible": true, "color_order": "bgr" } }
  ],
  "outputs": [
    { "id": 2, "name_zh": "输出图像", "type": "image",
      "relations": { "same_geometry_as": { "stage": "inputs", "id": 2 } },
      "display": { "visible": true, "color_order": "bgr" } },
    { "id": 3, "name_zh": "接收数据",
      "type_from": { "flow": { "stage": "init", "id": 0 }, "node": { "stage": "init", "id": 1 }, "port": { "stage": "init", "id": 2 } } }
  ]
})";

} // namespace

int main()
try
{
    const auto declaration = readToolDeclaration(kOrdinaryTool, "普通工具.json");
    require(declaration.nameEn == "test.ordinary" && declaration.nameZh == "普通工具", "identity");
    require(declaration.visibility == ToolVisibility::Internal, "visibility code");
    require(declaration.nodeModel == NodeModel::Select, "node model code");
    require(declaration.inputs.size() == 2 && declaration.outputs.size() == 2, "port counts");

    const auto &threshold = declaration.inputs[0];
    require(std::get<std::int64_t>(threshold.defaultValue) == 9007199254740993LL, "exact int64 default");
    require(threshold.editor == EditorKind::Number && threshold.tab == ParameterTab::Detection, "ui codes");
    require(threshold.group == "检测" && threshold.step && threshold.decimals.value_or(-1) == 2, "ui extras");
    require(threshold.minimum && threshold.maximum && threshold.options.size() == 2, "constraints");

    const auto &derived = declaration.outputs[1];
    require(derived.typeFrom && derived.typeFrom->nodeParameter == PortRef{PortStage::Init, 1}, "type_from");

    const auto written  = writeToolDeclaration(declaration);
    const auto restored = readToolDeclaration(written, "回写.json");
    require(restored.nameZh == declaration.nameZh && restored.inputs.size() == declaration.inputs.size(), "reopen");
    require(std::get<std::int64_t>(restored.inputs[0].defaultValue) == 9007199254740993LL, "exact int64 after rewrite");
    require(restored.outputs[1].typeFrom && restored.outputs[1].typeFrom->flowParameter == PortRef{PortStage::Init, 0},
            "type_from after rewrite");
    require(writeToolDeclaration(restored) == written, "rewrite is stable");

    // Array elements keep the same exactness: a dense int64 vector stores each digit, not a double.
    const auto arrayTool = readToolDeclaration(R"({"schema_version":2,"name_en":"a","name_zh":"b","version":"1",
        "implementation":{"kind":"cpp"},"inputs":[{"id":1,"name_zh":"序列","type":{"type":"array","dtype":"int64","rank":1},
        "default":[9007199254740993,9007199254740995]}]})");
    {
        const auto &elements = std::get<Array>(arrayTool.inputs[0].defaultValue).elements<std::int64_t>();
        require(elements[0] == 9007199254740993LL && elements[1] == 9007199254740995LL, "exact int64 elements");
        const auto  reopened = readToolDeclaration(writeToolDeclaration(arrayTool), "数组.json");
        const auto &again    = std::get<Array>(reopened.inputs[0].defaultValue).elements<std::int64_t>();
        require(again[0] == 9007199254740993LL && again[1] == 9007199254740995LL, "exact int64 elements after rewrite");
    }

    // Reserved ports are the host's, never the author's, so the reader refuses them by number.
    rejects(
        []
        {
            readToolDeclaration(R"({"schema_version":2,"name_en":"a","name_zh":"b","version":"1",
        "implementation":{"kind":"cpp"},"outputs":[{"id":0,"name_zh":"状态码","type":"int"}]})");
        },
        "宿主公共端口");
    rejects(
        []
        {
            readToolDeclaration(R"({"schema_version":2,"name_en":"a","name_zh":"b","version":"1",
        "implementation":{"kind":"cpp"},"inputs":[{"id":0,"name_zh":"状态","type":"int"}]})");
        },
        "宿主公共端口");

    // A decimal in an integer position is a different number, not the same one with a prettier text.
    rejects(
        []
        {
            readToolDeclaration(R"({"schema_version":2,"name_en":"a","name_zh":"b","version":"1",
        "implementation":{"kind":"cpp"},"inputs":[{"id":1,"name_zh":"x","type":"int","default":1.0}]})");
        },
        "必须是整数");
    rejects(
        []
        {
            readToolDeclaration(R"({"schema_version":2,"name_en":"a","name_zh":"b","version":"1",
        "implementation":{"kind":"cpp"},"inputs":[{"id":1,"name_zh":"x","type":"int","default":18446744073709551616}]})");
        },
        "int64");

    // An unknown field names the JSON path, and a duplicated key is not silently collapsed.
    rejects(
        []
        {
            readToolDeclaration(R"({"schema_version":2,"name_en":"a","name_zh":"b","version":"1",
        "implementation":{"kind":"cpp"},"inputs":[{"id":1,"name_zh":"x","type":"int","colour":1}]})");
        },
        "未知字段");
    rejects(
        []
        {
            readToolDeclaration(R"({"schema_version":2,"name_en":"a","name_en":"b","name_zh":"b","version":"1",
        "implementation":{"kind":"cpp"}})");
        },
        "重复字段");
    rejects(
        []
        {
            readToolDeclaration(R"({"schema_version":2,"name_en":"a","name_zh":"b","version":"1",
        "implementation":{"kind":"cpp"},"node_model":9})");
        },
        "枚举名称");

    rejects(
        []
        {
            readToolDeclaration(
                R"({"schema_version":1,"name_en":"old","name_zh":"旧声明","version":"1","implementation":{"kind":0}})");
        },
        "schema_version");
    const auto sharedRegions = readToolDeclaration(
        R"({"schema_version":2,"name_en":"align","name_zh":"对齐","version":"1","implementation":{"kind":"cpp"},"inputs":[{"id":1,"name_zh":"图像","type":"image"},{"id":6,"name_zh":"搜索区域","type":"roi","relations":{"image":{"stage":"inputs","id":1}}}],"training":{"writes":[{"source_port":2,"target":{"stage":"inputs","id":6}}],"image":{"stage":"inputs","id":1},"region_input":6,"records":{"max":1,"fields":[]},"call":{"tool":"trainer","inputs":[{"port":2,"source":{"stage":"inputs","id":6}}]}}})");
    require(readToolDeclaration(writeToolDeclaration(sharedRegions)).training->regionInput == 6,
            "training canvas references the existing business input through a round trip");
    rejects(
        [&]
        {
            auto invalid                  = sharedRegions;
            invalid.training->regionInput = 99;
            readToolDeclaration(writeToolDeclaration(invalid));
        },
        "region_input");
    auto trainer = readToolDeclaration(
        R"({"schema_version":2,"name_en":"trainer","name_zh":"训练器","version":"1","implementation":{"kind":"cpp"},"inputs":[{"id":1,"name_zh":"训练集","type":"training_set"}]})");
    trainer.inputs[0].defaultValue = defaultValue(trainer.inputs[0].type);
    require(isMissing(trainer.inputs[0].defaultValue), "a training set has no literal default");
    require(isMissing(readToolDeclaration(writeToolDeclaration(trainer)).inputs[0].defaultValue),
            "a normalized training input can be written and reopened");
    rejects(
        []
        {
            readToolDeclaration(
                R"({"schema_version":2,"name_en":"a","name_zh":"b","version":"1","implementation":{"kind":"cpp"},"inputs":[{"id":1,"name_zh":"区域","type":"roi","relations":{"image":{"stage":"inputs","id":-1}}}]})");
        },
        "32 位无符号整数");
    std::cout << "declaration read/write round-trip keeps exact integers and rejects at the field\n";
    return 0;
}
catch (const std::exception &error)
{
    std::cerr << error.what() << '\n';
    return 1;
}
