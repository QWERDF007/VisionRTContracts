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
  "schema_version": 1,
  "name_en": "test.ordinary",
  "name_zh": "普通工具",
  "version": "1.0.0",
  "implementation": { "kind": 0 },
  "ui": { "visibility": 1 },
  "node_model": 3,
  "init_params": [
    { "id": 0, "name_zh": "模型路径", "type": { "type": 3 } }
  ],
  "inputs": [
    { "id": 1, "name_zh": "阈值", "type": { "type": 0 },
      "default": 9007199254740993,
      "constraints": { "min": 0, "max": 1000, "options": [1, 2] },
      "ui": { "editor": 1, "tab": 2, "group": "检测", "step": 1, "decimals": 2 } },
    { "id": 2, "name_zh": "输入图像", "type": { "type": 4 },
      "display": { "visible": true, "color_order": 2 } }
  ],
  "outputs": [
    { "id": 2, "name_zh": "输出图像", "type": { "type": 4 },
      "relations": { "same_geometry_as": { "stage": 1, "id": 2 } },
      "display": { "visible": true, "color_order": 2 } },
    { "id": 3, "name_zh": "接收数据",
      "type_from": { "flow": { "stage": 0, "id": 0 }, "node": { "stage": 0, "id": 1 }, "port": { "stage": 0, "id": 2 } } }
  ]
})";

} // namespace

int main()
try
{
    const auto declaration = readToolDeclaration(kOrdinaryTool, "普通工具.json");
    require(declaration.nameEn == "test.ordinary" && declaration.nameZh == "普通工具", "identity");
    require(declaration.visibility == ToolVisibility::Assist, "visibility code");
    require(declaration.nodeModel == NodeModel::Select, "node model code");
    require(declaration.inputs.size() == 2 && declaration.outputs.size() == 2, "port counts");

    const auto &threshold = declaration.inputs[0];
    require(std::get<std::int64_t>(threshold.defaultValue) == 9007199254740993LL, "exact int64 default");
    require(threshold.editor == EditorKind::Number && threshold.tab == ParameterTab::Detection, "ui codes");
    require(threshold.group == "检测" && threshold.step && threshold.decimals.value_or(-1) == 2, "ui extras");
    require(threshold.minimum && threshold.maximum && threshold.options.size() == 2, "constraints");

    const auto &derived = declaration.outputs[1];
    require(derived.typeFrom && derived.typeFrom->nodeParameter == PortRef{PortStage::Init, 1}, "type_from");

    const auto written   = writeToolDeclaration(declaration);
    const auto restored  = readToolDeclaration(written, "回写.json");
    require(restored.nameZh == declaration.nameZh && restored.inputs.size() == declaration.inputs.size(), "reopen");
    require(std::get<std::int64_t>(restored.inputs[0].defaultValue) == 9007199254740993LL, "exact int64 after rewrite");
    require(restored.outputs[1].typeFrom
                && restored.outputs[1].typeFrom->flowParameter == PortRef{PortStage::Init, 0},
            "type_from after rewrite");
    require(writeToolDeclaration(restored) == written, "rewrite is stable");

    // Array elements keep the same exactness: a dense int64 vector stores each digit, not a double.
    const auto arrayTool = readToolDeclaration(R"({"schema_version":1,"name_en":"a","name_zh":"b","version":"1",
        "implementation":{"kind":0},"inputs":[{"id":1,"name_zh":"序列","type":{"type":5,"dtype":2,"rank":1},
        "default":[9007199254740993,9007199254740995]}]})");
    {
        const auto &elements = std::get<Array>(arrayTool.inputs[0].defaultValue).elements<std::int64_t>();
        require(elements[0] == 9007199254740993LL && elements[1] == 9007199254740995LL, "exact int64 elements");
        const auto reopened = readToolDeclaration(writeToolDeclaration(arrayTool), "数组.json");
        const auto &again    = std::get<Array>(reopened.inputs[0].defaultValue).elements<std::int64_t>();
        require(again[0] == 9007199254740993LL && again[1] == 9007199254740995LL, "exact int64 elements after rewrite");
    }

    // Reserved ports are the host's, never the author's, so the reader refuses them by number.
    rejects([] { readToolDeclaration(R"({"schema_version":1,"name_en":"a","name_zh":"b","version":"1",
        "implementation":{"kind":0},"outputs":[{"id":0,"name_zh":"状态码","type":{"type":0}}]})"); },
            "宿主公共端口");
    rejects([] { readToolDeclaration(R"({"schema_version":1,"name_en":"a","name_zh":"b","version":"1",
        "implementation":{"kind":0},"inputs":[{"id":0,"name_zh":"状态","type":{"type":0}}]})"); },
            "宿主公共端口");

    // A decimal in an integer position is a different number, not the same one with a prettier text.
    rejects([] { readToolDeclaration(R"({"schema_version":1,"name_en":"a","name_zh":"b","version":"1",
        "implementation":{"kind":0},"inputs":[{"id":1,"name_zh":"x","type":{"type":0},"default":1.0}]})"); },
            "必须是整数");
    rejects([] { readToolDeclaration(R"({"schema_version":1,"name_en":"a","name_zh":"b","version":"1",
        "implementation":{"kind":0},"inputs":[{"id":1,"name_zh":"x","type":{"type":0},"default":18446744073709551616}]})"); },
            "int64");

    // An unknown field names the JSON path, and a duplicated key is not silently collapsed.
    rejects([] { readToolDeclaration(R"({"schema_version":1,"name_en":"a","name_zh":"b","version":"1",
        "implementation":{"kind":0},"inputs":[{"id":1,"name_zh":"x","type":{"type":0},"colour":1}]})"); },
            "未知字段");
    rejects([] { readToolDeclaration(R"({"schema_version":1,"name_en":"a","name_en":"b","name_zh":"b","version":"1",
        "implementation":{"kind":0}})"); },
            "重复字段");
    rejects([] { readToolDeclaration(R"({"schema_version":1,"name_en":"a","name_zh":"b","version":"1",
        "implementation":{"kind":0},"node_model":9})"); },
            "枚举编码");

    std::cout << "declaration read/write round-trip keeps exact integers and rejects at the field\n";
    return 0;
}
catch (const std::exception &error)
{
    std::cerr << error.what() << '\n';
    return 1;
}
