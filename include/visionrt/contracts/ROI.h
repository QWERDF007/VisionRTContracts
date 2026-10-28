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

// Bounding box of an encoded ROI after mapping it through a row-major 3x3 reference-to-image
// matrix. The supported range is one finite invertible affine map, the same range the tools'
// alignment declares: circles become ellipses with an exact axis-aligned box, and a projective or
// singular map throws instead of returning a box that does not contain the image. Returns
// {minimum, maximum} in the target frame.
VT_EXPORT std::array<cv::Point2d, 2> transformedRoiBounds(std::span<const double> encoded,
                                                         std::span<const double> matrix);
VT_EXPORT RoiKind roiKind(std::span<const double> encoded);
VT_EXPORT std::array<cv::Point2d, 4> rectangleCorners(std::span<const double> encoded);

}
