#include "visionrt/contracts/ROI.h"
#include "visionrt/contracts/TypeSpec.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace visionrt {
namespace {

constexpr std::size_t kPointSize     = 3;
constexpr std::size_t kSegmentSize   = 5;
constexpr std::size_t kRectangleSize = 6;
constexpr std::size_t kCircleSize    = 4;
constexpr std::size_t kPolygonHead   = 1;
constexpr std::size_t kPointSetHead  = 1;

void requireKind(RoiKind actual, RoiKind expected, const char *message)
{
    if (actual != expected)
        throw ContractError(message);
}

std::size_t pairCount(std::span<const double> encoded) noexcept
{
    return (encoded.size() - 1) / 2;
}

cv::Point2d pairAt(std::span<const double> encoded, std::size_t index)
{
    return {encoded[1 + index * 2], encoded[2 + index * 2]};
}

EncodedRegion makePairs(RoiKind kind, std::span<const cv::Point2d> points)
{
    EncodedRegion encoded;
    encoded.reserve(1 + points.size() * 2);
    encoded.push_back(static_cast<double>(kind));
    for (const auto &point : points)
    {
        encoded.push_back(point.x);
        encoded.push_back(point.y);
    }
    return encoded;
}

void writePairs(EncodedRegion &encoded, std::span<const cv::Point2d> points)
{
    encoded.resize(1 + points.size() * 2);
    for (std::size_t index = 0; index < points.size(); ++index)
    {
        encoded[1 + index * 2] = points[index].x;
        encoded[2 + index * 2] = points[index].y;
    }
}

std::array<cv::Point2d, 4> rectangleCornersOf(std::span<const double> encoded)
{
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

} // namespace

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
        if (encoded.size() != kPointSize)
            throw ContractError("Point ROI requires 3 elements");
    }
    else if (code == 1)
    {
        if (encoded.size() != kSegmentSize)
            throw ContractError("Segment ROI requires 5 elements");
    }
    else if (code == 2)
    {
        if (encoded.size() != kRectangleSize)
            throw ContractError("Rectangle ROI requires 6 elements");
        if (encoded[3] <= 0 || encoded[4] <= 0)
            throw ContractError("Rectangle ROI width and height must be positive");
    }
    else if (code == 3)
    {
        if (encoded.size() != kCircleSize)
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

PointView::PointView(std::span<const double> encoded)
    : encoded_(encoded)
{
    if (encoded_.size() != kPointSize || static_cast<int>(encoded_[0]) != static_cast<int>(RoiKind::Point))
        throw ContractError("Point ROI requires 3 elements");
}

double PointView::x() const noexcept
{
    return encoded_[1];
}

double PointView::y() const noexcept
{
    return encoded_[2];
}

PointView::operator cv::Point2d() const
{
    return {x(), y()};
}

SegmentView::SegmentView(std::span<const double> encoded)
    : encoded_(encoded)
{
    if (encoded_.size() != kSegmentSize || static_cast<int>(encoded_[0]) != static_cast<int>(RoiKind::Segment))
        throw ContractError("Segment ROI requires 5 elements");
}

cv::Point2d SegmentView::start() const
{
    return {encoded_[1], encoded_[2]};
}

cv::Point2d SegmentView::end() const
{
    return {encoded_[3], encoded_[4]};
}

RectangleView::RectangleView(std::span<const double> encoded)
    : encoded_(encoded)
{
    if (encoded_.size() != kRectangleSize || static_cast<int>(encoded_[0]) != static_cast<int>(RoiKind::Rectangle))
        throw ContractError("Rectangle ROI requires 6 elements");
}

double RectangleView::left() const noexcept
{
    return encoded_[1];
}

double RectangleView::top() const noexcept
{
    return encoded_[2];
}

double RectangleView::width() const noexcept
{
    return encoded_[3];
}

double RectangleView::height() const noexcept
{
    return encoded_[4];
}

double RectangleView::angle() const noexcept
{
    return encoded_[5];
}

cv::Point2d RectangleView::center() const
{
    return {left() + width() / 2.0, top() + height() / 2.0};
}

std::array<cv::Point2d, 4> RectangleView::corners() const
{
    return rectangleCornersOf(encoded_);
}

CircleView::CircleView(std::span<const double> encoded)
    : encoded_(encoded)
{
    if (encoded_.size() != kCircleSize || static_cast<int>(encoded_[0]) != static_cast<int>(RoiKind::Circle))
        throw ContractError("Circle ROI requires 4 elements");
}

cv::Point2d CircleView::center() const
{
    return {encoded_[1], encoded_[2]};
}

double CircleView::radius() const noexcept
{
    return encoded_[3];
}

PointSetView::PointSetView(std::span<const double> encoded)
    : encoded_(encoded)
{
    if (encoded_.size() < kPointSetHead + 2 || encoded_.size() % 2 != 1
        || static_cast<int>(encoded_[0]) != static_cast<int>(RoiKind::PointSet))
        throw ContractError("Point-set ROI requires at least one coordinate pair");
}

std::size_t PointSetView::size() const noexcept
{
    return pairCount(encoded_);
}

cv::Point2d PointSetView::point(std::size_t index) const
{
    return pairAt(encoded_, index);
}

std::vector<cv::Point2d> PointSetView::points() const
{
    std::vector<cv::Point2d> result;
    result.reserve(size());
    for (std::size_t index = 0; index < size(); ++index)
        result.push_back(point(index));
    return result;
}

PolygonView::PolygonView(std::span<const double> encoded)
    : encoded_(encoded)
{
    if (encoded_.size() < 7 || encoded_.size() % 2 != 1
        || static_cast<int>(encoded_[0]) != static_cast<int>(RoiKind::Polygon))
        throw ContractError("Polygon ROI requires at least three coordinate pairs");
}

std::size_t PolygonView::size() const noexcept
{
    return pairCount(encoded_);
}

cv::Point2d PolygonView::point(std::size_t index) const
{
    return pairAt(encoded_, index);
}

std::vector<cv::Point2d> PolygonView::points() const
{
    std::vector<cv::Point2d> result;
    result.reserve(size());
    for (std::size_t index = 0; index < size(); ++index)
        result.push_back(point(index));
    return result;
}

RegionView::RegionView(std::span<const double> encoded)
    : encoded_(encoded)
{
    validateRoi(encoded_);
    kind_ = static_cast<RoiKind>(static_cast<int>(encoded_[0]));
}

RegionView RegionView::trusted(std::span<const double> encoded, RoiKind kind)
{
    RegionView view;
    view.encoded_ = encoded;
    view.kind_    = kind;
    return view;
}

RoiKind RegionView::kind() const noexcept
{
    return kind_;
}

bool RegionView::empty() const noexcept
{
    return encoded_.empty();
}

PointView RegionView::point() const
{
    requireKind(kind_, RoiKind::Point, "Region is not a point");
    return PointView(encoded_);
}

SegmentView RegionView::segment() const
{
    requireKind(kind_, RoiKind::Segment, "Region is not a segment");
    return SegmentView(encoded_);
}

RectangleView RegionView::rectangle() const
{
    requireKind(kind_, RoiKind::Rectangle, "Region is not a rectangle");
    return RectangleView(encoded_);
}

CircleView RegionView::circle() const
{
    requireKind(kind_, RoiKind::Circle, "Region is not a circle");
    return CircleView(encoded_);
}

PolygonView RegionView::polygon() const
{
    requireKind(kind_, RoiKind::Polygon, "Region is not a polygon");
    return PolygonView(encoded_);
}

PointSetView RegionView::pointSet() const
{
    requireKind(kind_, RoiKind::PointSet, "Region is not a point set");
    return PointSetView(encoded_);
}

std::span<const double> RegionView::encoded() const noexcept
{
    return encoded_;
}

EncodedRegion makePoint(double x, double y)
{
    return {static_cast<double>(RoiKind::Point), x, y};
}

EncodedRegion makeSegment(cv::Point2d start, cv::Point2d end)
{
    return {static_cast<double>(RoiKind::Segment), start.x, start.y, end.x, end.y};
}

EncodedRegion makeRectangle(double left, double top, double width, double height, double angleDegrees)
{
    if (!(width > 0) || !(height > 0))
        throw ContractError("Rectangle ROI width and height must be positive");
    return {static_cast<double>(RoiKind::Rectangle), left, top, width, height, angleDegrees};
}

EncodedRegion makeCircle(cv::Point2d center, double radius)
{
    if (!(radius > 0))
        throw ContractError("Circle ROI radius must be positive");
    return {static_cast<double>(RoiKind::Circle), center.x, center.y, radius};
}

EncodedRegion makePolygon(std::span<const cv::Point2d> points)
{
    if (points.size() < 3)
        throw ContractError("Polygon ROI requires at least three coordinate pairs");
    return makePairs(RoiKind::Polygon, points);
}

EncodedRegion makePointSet(std::span<const cv::Point2d> points)
{
    if (points.empty())
        throw ContractError("Point-set ROI requires at least one coordinate pair");
    return makePairs(RoiKind::PointSet, points);
}

RegionEditor::RegionEditor(EncodedRegion &encoded)
    : encoded_(encoded)
    , kind_(RegionView(encoded).kind())
{
}

RoiKind RegionEditor::kind() const noexcept
{
    return kind_;
}

void RegionEditor::translate(cv::Point2d delta)
{
    switch (kind_)
    {
    case RoiKind::Point:
        encoded_[1] += delta.x;
        encoded_[2] += delta.y;
        break;
    case RoiKind::Segment:
        encoded_[1] += delta.x;
        encoded_[2] += delta.y;
        encoded_[3] += delta.x;
        encoded_[4] += delta.y;
        break;
    case RoiKind::Rectangle:
        encoded_[1] += delta.x;
        encoded_[2] += delta.y;
        break;
    case RoiKind::Circle:
        encoded_[1] += delta.x;
        encoded_[2] += delta.y;
        break;
    case RoiKind::Polygon:
    case RoiKind::PointSet:
        for (std::size_t index = 0; index < pairCount(encoded_); ++index)
        {
            encoded_[1 + index * 2] += delta.x;
            encoded_[2 + index * 2] += delta.y;
        }
        break;
    }
}

void RegionEditor::moveTo(cv::Point2d value)
{
    requireKind(kind_, RoiKind::Point, "Region is not a point");
    encoded_[1] = value.x;
    encoded_[2] = value.y;
}

void RegionEditor::setVertex(std::size_t index, cv::Point2d value)
{
    switch (kind_)
    {
    case RoiKind::Point:
        if (index != 0)
            throw ContractError("Point ROI has one vertex");
        encoded_[1] = value.x;
        encoded_[2] = value.y;
        break;
    case RoiKind::Segment:
        if (index > 1)
            throw ContractError("Segment ROI has two vertices");
        encoded_[1 + index * 2] = value.x;
        encoded_[2 + index * 2] = value.y;
        break;
    case RoiKind::Polygon:
    case RoiKind::PointSet:
        if (index >= pairCount(encoded_))
            throw ContractError("ROI vertex index is out of range");
        encoded_[1 + index * 2] = value.x;
        encoded_[2 + index * 2] = value.y;
        break;
    case RoiKind::Rectangle:
    case RoiKind::Circle:
        throw ContractError("This ROI kind has constrained handles, not free vertices");
    }
}

void RegionEditor::setSegment(cv::Point2d start, cv::Point2d end)
{
    requireKind(kind_, RoiKind::Segment, "Region is not a segment");
    encoded_[1] = start.x;
    encoded_[2] = start.y;
    encoded_[3] = end.x;
    encoded_[4] = end.y;
}

void RegionEditor::setRectangle(double left, double top, double width, double height, double angleDegrees)
{
    requireKind(kind_, RoiKind::Rectangle, "Region is not a rectangle");
    if (!(width > 0) || !(height > 0))
        throw ContractError("Rectangle ROI width and height must be positive");
    encoded_[1] = left;
    encoded_[2] = top;
    encoded_[3] = width;
    encoded_[4] = height;
    encoded_[5] = angleDegrees;
}

void RegionEditor::setCircle(cv::Point2d center, double radius)
{
    requireKind(kind_, RoiKind::Circle, "Region is not a circle");
    if (!(radius > 0))
        throw ContractError("Circle ROI radius must be positive");
    encoded_[1] = center.x;
    encoded_[2] = center.y;
    encoded_[3] = radius;
}

void RegionEditor::setPolygon(std::span<const cv::Point2d> points)
{
    requireKind(kind_, RoiKind::Polygon, "Region is not a polygon");
    if (points.size() < 3)
        throw ContractError("Polygon ROI requires at least three coordinate pairs");
    writePairs(encoded_, points);
}

void RegionEditor::setPointSet(std::span<const cv::Point2d> points)
{
    requireKind(kind_, RoiKind::PointSet, "Region is not a point set");
    if (points.empty())
        throw ContractError("Point-set ROI requires at least one coordinate pair");
    writePairs(encoded_, points);
}

RoiKind roiKind(std::span<const double> encoded)
{
    validateRoi(encoded);
    return static_cast<RoiKind>(static_cast<int>(encoded[0]));
}

std::array<cv::Point2d, 4> rectangleCorners(std::span<const double> encoded)
{
    const auto view = RegionView(encoded);
    requireKind(view.kind(), RoiKind::Rectangle, "ROI is not a rectangle");
    return view.rectangle().corners();
}

std::array<cv::Point2d, 2> transformedRoiBounds(std::span<const double> encoded, std::span<const double> matrix)
{
    if (matrix.size() != 9)
        throw ContractError("ROI transform requires a 3x3 matrix");
    // One non-finite entry would poison every comparison below and leave an arbitrary corner as the
    // reported box, so the matrix itself is checked before any geometry runs.
    for (const double value : matrix)
        if (!std::isfinite(value))
            throw ContractError("ROI transform matrix must be finite");

    // Supported range: a finite invertible affine map - the same range the tools' alignment
    // declares. A projective map sends part of a region to infinity, so a per-corner box reports a
    // finite rectangle that does not contain the image. That capability needs its own
    // specification (sign and zero-crossing analysis of the denominator), and approximating it here
    // would be a guarantee the function cannot keep.
    if (matrix[6] != 0.0 || matrix[7] != 0.0)
        throw ContractError("ROI transform must be affine: projective maps are not supported");
    // det([[a, b], [d, e]]) == 0 collapses the plane; a zero homogeneous scale has no finite image
    // of any point. Both are refused before any geometry runs.
    if (matrix[0] * matrix[4] - matrix[1] * matrix[3] == 0.0 || matrix[8] == 0.0)
        throw ContractError("ROI transform is singular: the ROI has no image");

    const auto map = [&](cv::Point2d point)
    {
        const double      scale = matrix[8];
        const cv::Point2d target{(matrix[0] * point.x + matrix[1] * point.y + matrix[2]) / scale,
                                 (matrix[3] * point.x + matrix[4] * point.y + matrix[5]) / scale};
        if (!std::isfinite(target.x) || !std::isfinite(target.y))
            throw ContractError("ROI transform produced non-finite coordinates");
        return target;
    };
    const auto region = RegionView(encoded);
    std::vector<cv::Point2d> mapped;
    switch (region.kind())
    {
    case RoiKind::Circle:
    {
        // An affine image of a circle is an ellipse. Its half-extent along x is the radius times
        // the norm of the first row of the linear part (the support function of the unit disc), and
        // along y the second row - exact, with no sampling error.
        const auto        circle     = region.circle();
        const double      radius     = circle.radius();
        const cv::Point2d center     = map(circle.center());
        const double      scale      = std::abs(matrix[8]);
        const double      halfWidth  = radius * std::hypot(matrix[0], matrix[1]) / scale;
        const double      halfHeight = radius * std::hypot(matrix[3], matrix[4]) / scale;
        mapped = {{center.x - halfWidth, center.y - halfHeight},
                  {center.x + halfWidth, center.y + halfHeight}};
        break;
    }
    case RoiKind::Rectangle:
        for (const auto &corner : region.rectangle().corners())
            mapped.push_back(map(corner));
        break;
    case RoiKind::Point:
        mapped.push_back(map(region.point()));
        break;
    case RoiKind::Segment:
    {
        const auto segment = region.segment();
        mapped.push_back(map(segment.start()));
        mapped.push_back(map(segment.end()));
        break;
    }
    case RoiKind::Polygon:
    case RoiKind::PointSet:
    {
        const auto points = region.kind() == RoiKind::Polygon ? region.polygon().points() : region.pointSet().points();
        for (const auto &point : points)
            mapped.push_back(map(point));
        break;
    }
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
