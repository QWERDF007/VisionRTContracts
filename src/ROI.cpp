#include "visionrt/contracts/ROI.h"
#include "visionrt/contracts/TypeSpec.h"

#include <algorithm>
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

std::array<cv::Point2d, 2> transformedRoiBounds(std::span<const double> encoded, std::span<const double> matrix)
{
    if (matrix.size() != 9)
        throw ContractError("ROI transform requires a 3x3 matrix");
    const auto map = [&](cv::Point2d point)
    {
        const double w = matrix[6] * point.x + matrix[7] * point.y + matrix[8];
        const double d = w == 0.0 ? 1.0 : w;
        return cv::Point2d{(matrix[0] * point.x + matrix[1] * point.y + matrix[2]) / d,
                           (matrix[3] * point.x + matrix[4] * point.y + matrix[5]) / d};
    };
    std::vector<cv::Point2d> mapped;
    switch (roiKind(encoded))
    {
    case RoiKind::Circle:
    {
        const cv::Point2d center = map({encoded[1], encoded[2]});
        const cv::Point2d edge   = map({encoded[1] + encoded[3], encoded[2]});
        const double      radius = std::hypot(edge.x - center.x, edge.y - center.y);
        mapped = {{center.x - radius, center.y - radius}, {center.x + radius, center.y + radius}};
        break;
    }
    case RoiKind::Rectangle:
        for (const auto &corner : rectangleCorners(encoded))
            mapped.push_back(map(corner));
        break;
    default:
        for (std::size_t index = 1; index + 1 < encoded.size(); index += 2)
            mapped.push_back(map({encoded[index], encoded[index + 1]}));
        break;
    }
    if (mapped.empty())
        throw ContractError("ROI has no geometry to transform");
    cv::Point2d minimum = mapped.front();
    cv::Point2d maximum = mapped.front();
    for (const auto &point : mapped)
    {
        minimum.x = std::min(minimum.x, point.x);
        minimum.y = std::min(minimum.y, point.y);
        maximum.x = std::max(maximum.x, point.x);
        maximum.y = std::max(maximum.y, point.y);
    }
    return {minimum, maximum};
}

}
