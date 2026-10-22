#include "shared_probe.h"

#include <visionrt/contracts/Value.h>
#include <visionrt/contracts/Module.h>

#include <array>
#include <atomic>
#include <cstdint>

namespace {
std::atomic<int> imageReleases = 0;
}

int contractSharedProbe() {
    return static_cast<int>(visionrt::MODULE_ABI_MAJOR);
}

visionrt::DataRef<std::vector<cv::Mat>> contractSharedImages()
{
    auto pixels = std::shared_ptr<std::array<std::uint8_t, 6>>(new std::array<std::uint8_t, 6>{1, 2, 3, 4, 5, 6},
                                                               [](auto *storage)
                                                               {
                                                                   delete storage;
                                                                   ++imageReleases;
                                                               });
    return visionrt::DataRef<std::vector<cv::Mat>>({cv::Mat(2, 3, CV_8UC1, pixels->data())}, {pixels});
}

void mutateContractSharedImages(const visionrt::DataRef<std::vector<cv::Mat>> &images)
{
    images.get()[0].at<std::uint8_t>(1, 2) = 73;
    images.get().push_back(images.get()[0](cv::Rect(1, 0, 2, 2)));
}

int contractSharedImageReleases()
{
    return imageReleases.load();
}

visionrt::PortValue contractSharedArray()
{
    return visionrt::Array(visionrt::NestedVector<std::string, 3>{
        {{"共享数据"}, {}},
        {}
    });
}

visionrt::PortValue contractSharedRoi()
{
    return visionrt::DataRef<visionrt::ROI>(visionrt::ROI{{{0, 16777217.125, -2.5}}, {}});
}
