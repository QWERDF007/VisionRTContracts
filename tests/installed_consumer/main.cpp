#include "shared_probe.h"

#include <visionrt/contracts/Module.h>

#include <cassert>
#include <iostream>

using namespace visionrt;

int main()
try
{
    static_assert(MODULE_ABI_MAJOR == 3);
    const auto status = normalizeExecutionStatus({7, "\v\f", "installed detail"});
    assert(status.code == 7 && !status.message.empty() && status.detail == "installed detail");
    assert(contractSharedProbe() == 8);
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
    const PortValue rectangle = Roi::rectangle(1.25F, -2.5F, 3.75F, 4.5F, 17.5F);
    validateValue({Type::Rect}, rectangle);
    assert(typeOf(rectangle) == TypeSpec{Type::Rect});
    const auto integer = parseInt64("9007199254740993");
    assert(integer == 9007199254740993LL);
    assert(formatInt64(integer) == "9007199254740993");
    ImageBatch retained;
    {
        const auto source = OwnedImage::take(cv::Mat(3, 7, CV_8UC1, cv::Scalar(17)));
        retained          = ImageBatch({source.region({1, 0, 4, 3})});
    }
    assert(retained.images()[0].readOnlyMat().at<unsigned char>(2, 3) == 17);
    validateValue({Type::ImageBatch}, retained);
    std::cout << "Installed value ownership and tool interface verified\n";
    return 0;
}
catch (const std::exception &error)
{
    std::cerr << error.what() << '\n';
    return 1;
}
