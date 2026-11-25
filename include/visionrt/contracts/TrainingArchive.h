#pragma once

#include "visionrt/contracts/Export.h"
#include "visionrt/contracts/Tool.h"
#include "visionrt/contracts/TrainingSet.h"

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace visionrt {

struct TrainingArchiveImage
{
    PortId          datasetPort = 0;
    TrainingImageId imageId     = 0;
    ColorOrder      colorOrder  = ColorOrder::Automatic;
    cv::Mat         pixels;
};

class VT_EXPORT TrainingArchive
{
public:
    static constexpr int         kFormatVersion = 1;
    static constexpr const char *kManifestEntry = "manifest.json";

    static void write(const std::filesystem::path                             &path,
                      const std::map<std::string, std::vector<std::uint8_t>> &models,
                      const std::vector<TrainingArchiveImage>                 &images);

    static std::vector<std::uint8_t>         readModel(const std::filesystem::path &path, std::string_view artifactId);
    static std::vector<TrainingArchiveImage> readImages(const std::filesystem::path &path);
};

} // namespace visionrt
