#include "shared_probe.h"

#include <visionrt/contracts/Module.h>

#include <cassert>
#include <iostream>

using namespace visionrt;

int main()
try
{
    assert(contractSharedProbe() == 8);
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
