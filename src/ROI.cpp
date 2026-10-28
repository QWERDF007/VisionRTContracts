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

namespace {

// Only a matrix with a zero projective row keeps circles as ellipses; anything else maps the
// outline onto a general conic, where the ellipse formula no longer applies.
bool isAffine(std::span<const double> matrix)
{
    return matrix[6] == 0.0 && matrix[7] == 0.0;
}

} // namespace

std::array<cv::Point2d, 2> transformedRoiBounds(std::span<const double> encoded, std::span<const double> matrix)
{
    if (matrix.size() != 9)
        throw ContractError("ROI transform requires a 3x3 matrix");
    // One non-finite entry would poison every comparison below and leave an arbitrary corner as the
    // reported box, so the matrix itself is checked before any geometry runs.
    for (const double value : matrix)
        if (!std::isfinite(value))
            throw ContractError("ROI transform matrix must be finite");

    const auto map = [&](cv::Point2d point)
    {
        const double w = matrix[6] * point.x + matrix[7] * point.y + matrix[8];
        // A zero denominator means the point has no image in the target frame. The previous
        // fallback to 1 invented a coordinate that looked valid and then dragged the whole box onto
        // it; a singular alignment has to be reported instead of approximated.
        if (!std::isfinite(w) || w == 0.0)
            throw ContractError("ROI transform is singular: the ROI maps to infinity");
        const cv::Point2d target{(matrix[0] * point.x + matrix[1] * point.y + matrix[2]) / w,
                                 (matrix[3] * point.x + matrix[4] * point.y + matrix[5]) / w};
        if (!std::isfinite(target.x) || !std::isfinite(target.y))
            throw ContractError("ROI transform produced non-finite coordinates");
        return target;
    };
    std::vector<cv::Point2d> mapped;
    switch (roiKind(encoded))
    {
    case RoiKind::Circle:
    {
        const double      radius = encoded[3];
        const cv::Point2d center = map({encoded[1], encoded[2]});
        if (isAffine(matrix))
        {
            // The image of a circle is an ellipse. Its half-extent along x is the radius times the
            // norm of the first row of the linear part (the support function of the unit disc), and
            // along y the second row. Scaling a single radius by the x axis instead - the old
            // behaviour - over-reports the other axis as soon as the scale is not uniform.
            const double scale      = std::abs(matrix[8]);
            const double halfWidth  = radius * std::hypot(matrix[0], matrix[1]) / scale;
            const double halfHeight = radius * std::hypot(matrix[3], matrix[4]) / scale;
            mapped = {{center.x - halfWidth, center.y - halfHeight},
                      {center.x + halfWidth, center.y + halfHeight}};
            break;
        }
        // A projective image of a circle is a conic whose bounding box has no closed form worth the
        // risk here. Sampling the outline keeps the answer honest: 512 points push the
        // underestimate far below one pixel for any frame the host displays.
        constexpr int kCircleSamples = 512;
        mapped.reserve(kCircleSamples);
        for (int index = 0; index < kCircleSamples; ++index)
        {
            const double angle = 2.0 * std::numbers::pi * index / kCircleSamples;
            mapped.push_back(
                map({encoded[1] + radius * std::cos(angle), encoded[2] + radius * std::sin(angle)}));
        }
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
    // The analytic circle extents are computed outside map(), so the overflow case is closed here as
    // well: a box may only be returned when all four numbers are real.
    if (!std::isfinite(minimum.x) || !std::isfinite(minimum.y) || !std::isfinite(maximum.x)
        || !std::isfinite(maximum.y))
        throw ContractError("ROI transform produced non-finite coordinates");
    return {minimum, maximum};
}

}
