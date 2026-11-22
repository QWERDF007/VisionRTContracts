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

    // The authoring block keeps the workbench facts: a helper port takes either a context role or a
    // node port, and a click writes exactly the listed targets.
    {
        const auto authored = readToolDeclaration(R"({"schema_version":1,"name_en":"probe","name_zh":"探针",
            "version":"1","implementation":{"kind":0},
            "inputs":[{"id":21,"name_zh":"图像","type":{"type":4},"display":{"visible":true}},
                      {"id":34,"name_zh":"区域","type":{"type":6},"default":[]}],
            "outputs":[{"id":2,"name_zh":"计数","type":{"type":0}}],
            "authoring":{"version":1,"easy_touch":{
                "image":{"stage":1,"id":21},
                "call":{"tool":"ProbePick","inputs":[
                    {"port":1,"source":{"stage":1,"id":21}},
                    {"port":2,"context":0},
                    {"port":3,"context":1},
                    {"port":6,"context":2},
                    {"port":4,"source":{"stage":1,"id":34}}]},
                "preview_output":30,"valid_output":32,"message_output":33,
                "writes":[{"source_port":31,"target":{"stage":1,"id":34}},
                          {"source_port":34,"target":{"stage":0,"id":0}}]}}})", "探针.json");
        require(authored.authoring && authored.authoring->version == 1, "authoring version");
        const auto &touch = *authored.authoring->easyTouch;
        require(touch.image == PortRef{PortStage::Inputs, 21}, "easy_touch image");
        require(touch.call.tool == "ProbePick" && touch.call.inputs.size() == 5, "call identity");
        require(std::get<ContextRole>(touch.call.inputs[1].source) == ContextRole::Cursor, "cursor role");
        require(std::get<ContextRole>(touch.call.inputs[3].source) == ContextRole::Append, "append role");
        require(std::get<PortRef>(touch.call.inputs[4].source) == PortRef{PortStage::Inputs, 34}, "source port");
        require(touch.previewOutput == 30 && touch.validOutput == 32 && touch.messageOutput == 33, "read outputs");
        require(touch.writes.size() == 2 && touch.writes[1].target == PortRef{PortStage::Init, 0}, "writes");

        const auto  written  = writeToolDeclaration(authored);
        const auto  restored = readToolDeclaration(written, "回写.json");
        require(restored.authoring && restored.authoring->easyTouch, "authoring after rewrite");
        require(writeToolDeclaration(restored) == written, "authoring rewrite is stable");
        require(!written.empty() && written.find("\"easy_touch\"") != std::string::npos, "authoring is emitted");
    }

    // The training block names the frame "加入训练" copies and the typed fields a record carries.
    // Field ids are the trainer's keys, so a renamed field keeps its number and its value.
    {
        const auto trained = readToolDeclaration(R"({"schema_version":1,"name_en":"trainer","name_zh":"训练",
            "version":"1","implementation":{"kind":0},
            "init_params":[{"id":0,"name_zh":"训练数据路径","type":{"type":3}}],
            "inputs":[{"id":1,"name_zh":"图像","type":{"type":4}}],
            "outputs":[{"id":2,"name_zh":"计数","type":{"type":0}}],
            "training":{"kind":1,"asset_param":0,"image":{"stage":1,"id":1},
              "records":{"max":1,"fields":[
                {"id":1,"name_zh":"参考边","type":{"type":6},"roi":true,"regions":{"min":2,"max":2}},
                {"id":3,"name_zh":"备注","type":{"type":3}}]}}})", "训练.json");
        require(trained.training && trained.training->image == PortRef{PortStage::Inputs, 1},
                "training image");
        const auto &records = trained.training->records;
        require(records.maxCount == 1 && records.fields.size() == 2, "training record shape");
        require(records.roiField() == 1, "training canvas field");
        require(records.fields[0].minRegions == 2 && records.fields[0].maxRegions == 2,
                "training region budget");

        const auto written  = writeToolDeclaration(trained);
        const auto restored = readToolDeclaration(written, "回写.json");
        require(restored.training && restored.training->records == records, "training survives rewrite");
        require(writeToolDeclaration(restored) == written, "training rewrite is stable");
    }

    // The trainer may name one of its outputs as the geometry the training page draws. The host then
    // shows what the trainer measured instead of deriving endpoints from a model it cannot read.
    {
        const auto trained = readToolDeclaration(R"({"schema_version":1,"name_en":"edges","name_zh":"直边",
            "version":"1","implementation":{"kind":0},
            "init_params":[{"id":0,"name_zh":"训练数据","type":{"type":3}},
                           {"id":1,"name_zh":"模型","type":{"type":3}}],
            "inputs":[{"id":1,"name_zh":"图像","type":{"type":4}}],
            "training":{"kind":1,"asset_param":0,"image":{"stage":1,"id":1},
              "records":{"max":1,"fields":[{"id":1,"name_zh":"参考边","type":{"type":6},"roi":true,
                                            "regions":{"min":2,"max":2}}]},
              "call":{"tool":"TrainStraightBorders","inputs":[{"port":1,"context":4}]},
              "artifacts":[{"id":"edge-model","source_port":2,"format":"edge","version":1,
                            "target":{"stage":0,"id":1}}],
              "overlay_output":3}})", "直边.json");
        require(trained.training && trained.training->overlayOutput == 3, "training overlay output");
        const auto written = writeToolDeclaration(trained);
        const auto restored = readToolDeclaration(written, "回写.json");
        require(restored.training->overlayOutput == 3, "overlay output survives rewrite");
        require(writeToolDeclaration(restored) == written, "overlay rewrite is stable");
    }
    rejects([] { readToolDeclaration(R"({"schema_version":1,"name_en":"a","name_zh":"b","version":"1",
        "implementation":{"kind":0},"init_params":[{"id":0,"name_zh":"路径","type":{"type":3}}],
        "inputs":[{"id":1,"name_zh":"图像","type":{"type":4}}],
        "training":{"kind":1,"asset_param":0,"image":{"stage":1,"id":1},
          "records":{"fields":[{"id":1,"name_zh":"a","type":{"type":6},"roi":true}]},
          "overlay_output":3}})"); },
            "overlay_output 需要训练器调用");
    rejects([] { readToolDeclaration(R"({"schema_version":1,"name_en":"a","name_zh":"b","version":"1",
        "implementation":{"kind":0},"init_params":[{"id":0,"name_zh":"路径","type":{"type":3}}],
        "inputs":[{"id":1,"name_zh":"图像","type":{"type":4}}],
        "training":{"kind":0,"asset_param":0,"image":{"stage":1,"id":1},
          "records":{"fields":[{"id":1,"name_zh":"a","type":{"type":3}},
                               {"id":1,"name_zh":"b","type":{"type":3}}]}}})"); },
            "重复声明了字段编号");
    rejects([] { readToolDeclaration(R"({"schema_version":1,"name_en":"a","name_zh":"b","version":"1",
        "implementation":{"kind":0},"init_params":[{"id":0,"name_zh":"路径","type":{"type":3}}],
        "inputs":[{"id":1,"name_zh":"图像","type":{"type":4}}],
        "training":{"kind":0,"asset_param":0,"image":{"stage":1,"id":1},
          "records":{"fields":[{"id":1,"name_zh":"a","type":{"type":6},"roi":true},
                               {"id":2,"name_zh":"b","type":{"type":6},"roi":true}]}}})"); },
            "只能有一个画布 ROI 字段");
    rejects([] { readToolDeclaration(R"({"schema_version":1,"name_en":"a","name_zh":"b","version":"1",
        "implementation":{"kind":0},"init_params":[{"id":0,"name_zh":"路径","type":{"type":3}}],
        "inputs":[{"id":1,"name_zh":"图像","type":{"type":4}}],
        "training":{"kind":0,"asset_param":0,"image":{"stage":1,"id":9},
          "records":{"fields":[{"id":1,"name_zh":"a","type":{"type":3}}]}}})"); },
            "training.image");
    rejects([] { readToolDeclaration(R"({"schema_version":1,"name_en":"a","name_zh":"b","version":"1",
        "implementation":{"kind":0},"init_params":[{"id":0,"name_zh":"路径","type":{"type":3}}],
        "inputs":[{"id":1,"name_zh":"图像","type":{"type":4}}],
        "training":{"kind":0,"asset_param":0,"image":{"stage":1,"id":1}}})"); },
            "缺少 records");


    rejects([] { readToolDeclaration(R"({"schema_version":1,"name_en":"a","name_zh":"b","version":"1",
        "implementation":{"kind":0},"authoring":{"version":2,"easy_touch":{"image":{"stage":1,"id":1},
        "call":{"tool":"h"},"preview_output":2,"valid_output":4,
        "writes":[{"source_port":3,"target":{"stage":1,"id":2}}]}}})"); },
            "authoring.version");
    rejects([] { readToolDeclaration(R"({"schema_version":1,"name_en":"a","name_zh":"b","version":"1",
        "implementation":{"kind":0},"authoring":{"version":1}})"); },
            "至少声明一个作用域");
    rejects([] { readToolDeclaration(R"({"schema_version":1,"name_en":"a","name_zh":"b","version":"1",
        "implementation":{"kind":0},"authoring":{"version":1,"easy_touch":{"image":{"stage":1,"id":1},
        "call":{"tool":"h"},"preview_output":2,"valid_output":4,"writes":[]}}})"); },
            "writes");
    rejects([] { readToolDeclaration(R"({"schema_version":1,"name_en":"a","name_zh":"b","version":"1",
        "implementation":{"kind":0},"authoring":{"version":1,"easy_touch":{"image":{"stage":1,"id":1},
        "call":{"tool":"h","inputs":[{"port":2,"context":0,"source":{"stage":1,"id":1}}]},
        "preview_output":2,"valid_output":4,"writes":[{"source_port":3,"target":{"stage":1,"id":1}}]}}})"); },
            "必须且只能声明");
    rejects([] { readToolDeclaration(R"({"schema_version":1,"name_en":"a","name_zh":"b","version":"1",
        "implementation":{"kind":0},"authoring":{"version":1,"easy_touch":{"image":{"stage":1,"id":1},
        "call":{"tool":"h","inputs":[{"port":2,"context":0},{"port":2,"context":1}]},
        "preview_output":2,"valid_output":4,"writes":[{"source_port":3,"target":{"stage":1,"id":1}}]}}})"); },
            "重复声明");
    rejects([] { readToolDeclaration(R"({"schema_version":1,"name_en":"a","name_zh":"b","version":"1",
        "implementation":{"kind":0},"authoring":{"version":1,"easy_touch":{"image":{"stage":1,"id":1},
        "call":{"tool":"h"},"preview_output":2,"valid_output":4,
        "writes":[{"source_port":3,"target":{"stage":1,"id":1}},
                  {"source_port":5,"target":{"stage":1,"id":1}}]}}})"); },
            "重复写入");

    std::cout << "declaration read/write round-trip keeps exact integers and rejects at the field\n";
    return 0;
}
catch (const std::exception &error)
{
    std::cerr << error.what() << '\n';
    return 1;
}
