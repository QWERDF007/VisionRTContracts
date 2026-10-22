#pragma once

#include "visionrt/contracts/Export.h"
#include <opencv2/core.hpp>

#include <array>
#include <span>
#include <vector>

namespace visionrt {

using ROI = std::vector<std::vector<std::vector<double>>>;
enum class RoiKind
{
    Point     = 0,
    Segment   = 1,
    Rectangle = 2,
    Circle    = 3,
    Polygon   = 4,
    PointSet  = 5
};

VT_EXPORT void    validateRoi(std::span<const double> encoded);
VT_EXPORT RoiKind roiKind(std::span<const double> encoded);
VT_EXPORT std::array<cv::Point2d, 4> rectangleCorners(std::span<const double> encoded);

}
