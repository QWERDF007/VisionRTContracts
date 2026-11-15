#include "shared_probe.h"

#include <visionrt/contracts/Module.h>
#include <visionrt/contracts/FunctionTool.h>

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
    static_assert(MODULE_ABI_MAJOR == 10);
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
    std::cout << "Installed value ownership and tool interface verified\n";
    return 0;
}
catch (const std::exception &error)
{
    std::cerr << error.what() << '\n';
    return 1;
}
