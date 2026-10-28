#include "shared_probe.h"

#include <visionrt/contracts/Module.h>

#include <cassert>
#include <iostream>

using namespace visionrt;

int main()
try
{
    static_assert(MODULE_ABI_MAJOR == 7);
    const auto result = toolResult({{"value", std::int64_t{21}}}, {7, "业务失败", "installed detail"});
    assert(std::get<std::int64_t>(result.outputs.at("value")) == 21);
    // The status is one value on the result; the two ports are the host's to materialise, so a tool
    // never writes them into its own output set.
    assert(std::get<std::int64_t>(result.outputs.at(std::string(kStatusCodePort))) == 7 && std::get<std::string>(result.outputs.at(std::string(kStatusMessagePort))) == "业务失败");
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
    const auto arrayAlias                               = array;
    using Strings                                       = NestedVector<std::string, 3>;
    std::get<Array>(arrayAlias).get<Strings>()[0][0][0] = "消费者修改";
    assert(std::get<Array>(array).get<Strings>()[0][0][0] == "消费者修改");
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
