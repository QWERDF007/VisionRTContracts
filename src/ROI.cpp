#include "visionrt/contracts/ROI.h"
#include "visionrt/contracts/TypeSpec.h"

#include <cmath>
#include <numbers>

namespace visionrt {

void validateRoi(std::span<const double> encoded)
{
    if (encoded.empty())
        throw ContractError("ROI is missing its type code");
    for (const double value : encoded)
        if (!std::isfinite(value))
            throw ContractError("ROI coordinates and type code must be finite");
    const double code = encoded[0];
    if (code == 0)
    {
        if (encoded.size() != 3)
            throw ContractError("Point ROI requires 3 elements");
    }
    else if (code == 1)
    {
        if (encoded.size() != 5)
            throw ContractError("Segment ROI requires 5 elements");
    }
    else if (code == 2)
    {
        if (encoded.size() != 6)
            throw ContractError("Rectangle ROI requires 6 elements");
        if (encoded[3] <= 0 || encoded[4] <= 0)
            throw ContractError("Rectangle ROI width and height must be positive");
    }
    else if (code == 3)
    {
        if (encoded.size() != 4)
            throw ContractError("Circle ROI requires 4 elements");
        if (encoded[3] <= 0)
            throw ContractError("Circle ROI radius must be positive");
    }
    else if (code == 4)
    {
        if (encoded.size() < 7 || encoded.size() % 2 != 1)
            throw ContractError("Polygon ROI requires at least three coordinate pairs");
    }
    else if (code == 5)
    {
        if (encoded.size() < 3 || encoded.size() % 2 != 1)
            throw ContractError("Point-set ROI requires at least one coordinate pair");
    }
    else
    {
        throw ContractError("ROI type code must be an integer in [0,5]");
    }
}

RoiKind roiKind(std::span<const double> encoded)
{
    validateRoi(encoded);
    return static_cast<RoiKind>(static_cast<int>(encoded[0]));
}

std::array<cv::Point2d, 4> rectangleCorners(std::span<const double> encoded)
{
    if (roiKind(encoded) != RoiKind::Rectangle)
        throw ContractError("ROI is not a rectangle");
    const double               halfWidth  = encoded[3] / 2.0;
    const double               halfHeight = encoded[4] / 2.0;
    const double               centerX    = encoded[1] + halfWidth;
    const double               centerY    = encoded[2] + halfHeight;
    const double               radians    = std::remainder(encoded[5], 360.0) * std::numbers::pi / 180.0;
    const double               cosine     = std::cos(radians);
    const double               sine       = std::sin(radians);
    std::array<cv::Point2d, 4> corners{
        {{-halfWidth, -halfHeight}, {halfWidth, -halfHeight}, {halfWidth, halfHeight}, {-halfWidth, halfHeight}}
    };
    for (auto &point : corners)
    {
        const double oldX = point.x;
        point.x           = centerX + cosine * oldX - sine * point.y;
        point.y           = centerY + sine * oldX + cosine * point.y;
    }
    return corners;
}

}
