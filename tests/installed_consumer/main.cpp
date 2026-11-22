#include "shared_probe.h"

#include <visionrt/contracts/DeclarationJson.h>
#include <visionrt/contracts/FunctionTool.h>
#include <visionrt/contracts/Module.h>

#include <cassert>
#include <iostream>
#include <map>

using namespace visionrt;

namespace {
class ConsumerRegistry final : public IToolRegistry
{
public:
    std::map<std::string, ToolFactoryDescriptor> factories;
    ToolStatus add(const ToolFactoryDescriptor &factory) noexcept override
    {
        factories.emplace(factory.tool_id, factory);
        return {};
    }
};

class Counter final : public IToolInstance
{
public:
    ToolStatus initialize(const Values &) override { return {}; }
    ToolResult execute(const Values &) override { return toolResult({{2, ++count_}}, {}); }
    ToolStatus release() override { return {}; }
private:
    std::int64_t count_ = 0;
};

ToolResult echo(const Values &inputs) { return toolResult({{2, inputs.at(1)}}, {}); }
}

int main()
try
{
    static_assert(MODULE_ABI_MAJOR == 11);
    ConsumerRegistry registry;
    assert(registerClass<Counter>(registry, "Counter"));
    assert(registerFunction<&echo>(registry, "Echo"));
    ToolStatus created;
    const auto counterFactory = registry.factories.at("Counter");
    auto *first = counterFactory.create(&created);
    assert(first && created && first->initialize({}));
    auto *second = counterFactory.create(&created);
    assert(second && created && second->initialize({}));
    assert(std::get<std::int64_t>(first->execute({}).outputs.at(2)) == 1);
    assert(std::get<std::int64_t>(first->execute({}).outputs.at(2)) == 2);
    assert(std::get<std::int64_t>(second->execute({}).outputs.at(2)) == 1);
    assert(first->release() && second->release());
    counterFactory.destroy(first);
    counterFactory.destroy(second);
    const auto functionFactory = registry.factories.at("Echo");
    auto *function = functionFactory.create(&created);
    assert(function && created && function->initialize({}));
    assert(!function->initialize({{1, std::int64_t{4}}}));
    assert(std::get<std::int64_t>(function->execute({{1, std::int64_t{41}}}).outputs.at(2)) == 41);
    assert(function->release());
    functionFactory.destroy(function);
    const auto result = toolResult({{2, std::int64_t{21}}}, {7, "业务失败", "installed detail"});
    assert(std::get<std::int64_t>(result.outputs.at(2)) == 21);
    // The shared result helper materializes both status ports from the one tool status.
    assert(std::get<std::int64_t>(result.outputs.at(kStatusCodePort)) == 7
           && std::get<std::string>(result.outputs.at(kStatusMessagePort)) == "业务失败");
    assert(result.detail == "installed detail");


    assert(contractSharedProbe() == MODULE_ABI_MAJOR);
    assert(contractSharedImageReleases() == 0);
    {
        const auto images = contractSharedImages();
        const auto alias  = images;
        mutateContractSharedImages(alias);
        assert(&images.get() == &alias.get());
        assert(images.get().size() == 2);
        assert(images.get()[0].at<unsigned char>(1, 2) == 73);
        assert(images.get()[1].at<unsigned char>(1, 1) == 73);
        assert(images.get()[1].step[0] == images.get()[0].step[0]);
        assert(contractSharedImageReleases() == 0);
    }
    assert(contractSharedImageReleases() == 1);
    const auto array = contractSharedArray();
    validateValue({Type::Array, DType::String, 3}, array);
    const auto arrayAlias = array;
    std::get<Array>(arrayAlias).elements<std::string>()[0] = "消费者修改";
    assert(std::get<Array>(array).elements<std::string>()[0] == "消费者修改");
    const auto regions = contractSharedRoi();
    validateValue({Type::ROI}, regions);
    assert(std::get<DataRef<ROI>>(regions).get()[0][0][1] == 16777217.125);
    const auto regionsAlias = regions;
    std::get<DataRef<ROI>>(regionsAlias).get()[0][0][1] += 0.25;
    assert(std::get<DataRef<ROI>>(regions).get()[0][0][1] == 16777217.375);
    const PortValue rectangle = DataRef<ROI>(ROI{{{2, 1.25, -2.5, 3.75, 4.5, 17.5}}});
    validateValue({Type::ROI}, rectangle);
    assert(typeOf(rectangle) == TypeSpec{Type::ROI});
    const auto integer = parseInt64("9007199254740993");
    assert(integer == 9007199254740993LL);
    assert(formatInt64(integer) == "9007199254740993");
    Image retained;
    {
        const cv::Mat source(3, 7, CV_8UC1, cv::Scalar(17));
        retained = {source(cv::Rect(1, 0, 4, 3))};
    }
    assert(retained[0].at<unsigned char>(2, 3) == 17);
    validateValue({Type::Image}, DataRef<Image>(std::move(retained)));
    for (const auto &type : Array::supportedTypes())
    {
        auto value = Array::empty(*type.dtype, type.rank);
        validateValue(type, value);
        assert(typeOf(value) == type);
    }

    // A tool author's own build reads and rewrites a declaration with no GUI framework linked.
    const auto declaration = readToolDeclaration(R"({
        "schema_version": 2,
        "name_en": "consumer.tool",
        "name_zh": "消费者工具",
        "version": "1.0.0",
        "implementation": { "kind": "cpp" },
        "init_params": [ { "id": 0, "name_zh": "模型", "type": "string" } ],
        "inputs": [ { "id": 1, "name_zh": "阈值", "type": "int", "default": 9007199254740993 } ],
        "outputs": [ { "id": 2, "name_zh": "结果", "type": "int" } ]
    })");
    assert(declaration.nameEn == "consumer.tool" && declaration.inputs.size() == 1);
    assert(std::get<std::int64_t>(declaration.inputs[0].defaultValue) == 9007199254740993LL);
    const auto rewritten = writeToolDeclaration(declaration);
    const auto reopened  = readToolDeclaration(rewritten);
    assert(std::get<std::int64_t>(reopened.inputs[0].defaultValue) == 9007199254740993LL);
    assert(writeToolDeclaration(reopened) == rewritten);
    {
        // The training call value keeps exact keys and owners through an installed SDK.
        TrainingSet set;
        const cv::Mat frame(2, 3, CV_8UC1, cv::Scalar(9));
        set.images.emplace(4, trainingFrame(4, frame, ColorOrder::Gray));
        TrainingRecord record;
        record.id      = 3;
        record.imageId = 4;
        record.fields.emplace(1, std::int64_t{9007199254740993LL});
        set.records.emplace(record.id, record);
        validateTrainingSet(set);
        const PortValue value = DataRef<TrainingSet>(set);
        assert(typeOf(value) == TypeSpec{Type::TrainingSet});
        validateValue({Type::TrainingSet}, value);
        const auto retained = std::get<DataRef<TrainingSet>>(retainValue(value, std::make_shared<int>(0)));
        assert(std::get<std::int64_t>(retained.get().records.at(3).fields.at(1)) == 9007199254740993LL);
        assert(retained.get().images.at(4).pixels.at<unsigned char>(0, 0) == 9);
    }
    std::cout << "Installed value ownership, tool interface and declaration round-trip verified\n";
    return 0;
}
catch (const std::exception &error)
{
    std::cerr << error.what() << '\n';
    return 1;
}
