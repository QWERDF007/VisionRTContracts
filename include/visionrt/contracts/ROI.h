#pragma once

#include "visionrt/contracts/Export.h"
#include "visionrt/contracts/TypeSpec.h"
#include <opencv2/core.hpp>

#include <array>
#include <span>
#include <vector>

namespace visionrt {

using ROI = std::vector<std::vector<std::vector<double>>>;
// One ROI slot holds a group of regions; each region is one encoded geometry.
using EncodedRegion = std::vector<double>;
using RegionGroup   = std::vector<EncodedRegion>;

// The encoded layout every region uses: a leading kind code followed by coordinates. Rectangle and
// circle fields keep their original meaning - a rectangle stores its unrotated left/top corner with
// width, height and angle, never a centre.
enum class RoiKind
{
    Point     = 0,
    Segment   = 1,
    Rectangle = 2,
    Circle    = 3,
    Polygon   = 4,
    PointSet  = 5
};

VT_EXPORT void validateRoi(std::span<const double> encoded);

// Bounding box of an encoded ROI after mapping it through a row-major 3x3 reference-to-image
// matrix. The supported range is one finite invertible affine map, the same range the tools'
// alignment declares: circles become ellipses with an exact axis-aligned box, and a projective or
// singular map throws instead of returning a box that does not contain the image. Returns
// {minimum, maximum} in the target frame.
VT_EXPORT std::array<cv::Point2d, 2> transformedRoiBounds(std::span<const double> encoded,
                                                         std::span<const double> matrix);
VT_EXPORT RoiKind roiKind(std::span<const double> encoded);
VT_EXPORT std::array<cv::Point2d, 4> rectangleCorners(std::span<const double> encoded);

// Read-only, allocation-free views over one encoded region. A view borrows the encoded vector and
// stays valid only while that vector is neither modified nor reallocated.
class PointView
{
public:
    PointView() = default;
    explicit PointView(std::span<const double> encoded);

    double x() const noexcept;
    double y() const noexcept;
    operator cv::Point2d() const;

private:
    std::span<const double> encoded_;
};

class SegmentView
{
public:
    SegmentView() = default;
    explicit SegmentView(std::span<const double> encoded);

    cv::Point2d start() const;
    cv::Point2d end() const;

private:
    std::span<const double> encoded_;
};

class RectangleView
{
public:
    RectangleView() = default;
    explicit RectangleView(std::span<const double> encoded);

    double      left() const noexcept;
    double      top() const noexcept;
    double      width() const noexcept;
    double      height() const noexcept;
    double      angle() const noexcept;
    cv::Point2d center() const;
    // Same geometry as rectangleCorners(), computed from this view.
    std::array<cv::Point2d, 4> corners() const;

private:
    std::span<const double> encoded_;
};

class CircleView
{
public:
    CircleView() = default;
    explicit CircleView(std::span<const double> encoded);

    cv::Point2d center() const;
    double      radius() const noexcept;

private:
    std::span<const double> encoded_;
};

class PointSetView
{
public:
    PointSetView() = default;
    explicit PointSetView(std::span<const double> encoded);

    std::size_t      size() const noexcept;
    cv::Point2d      point(std::size_t index) const;
    std::vector<cv::Point2d> points() const;

private:
    std::span<const double> encoded_;
};

// Polygon shares the point-list layout; the type keeps the meaning visible at call sites.
class PolygonView
{
public:
    PolygonView() = default;
    explicit PolygonView(std::span<const double> encoded);

    std::size_t              size() const noexcept;
    cv::Point2d              point(std::size_t index) const;
    std::vector<cv::Point2d> points() const;

private:
    std::span<const double> encoded_;
};

// Validates the encoding once, then answers every named accessor from the cached kind: a draw or
// hit-test loop must not re-scan and re-validate the same region.
class RegionView
{
public:
    RegionView() = default;
    explicit RegionView(std::span<const double> encoded);
    // For data this module produced or already validated; no second scan.
    static RegionView trusted(std::span<const double> encoded, RoiKind kind);

    RoiKind kind() const noexcept;
    bool    empty() const noexcept;

    PointView     point() const;
    SegmentView   segment() const;
    RectangleView rectangle() const;
    CircleView    circle() const;
    PolygonView   polygon() const;
    PointSetView  pointSet() const;

    std::span<const double> encoded() const noexcept;

private:
    std::span<const double> encoded_;
    RoiKind                 kind_ = RoiKind::Point;
};

// Constructors produce the existing encoding, so storage and wire format never change.
VT_EXPORT EncodedRegion makePoint(double x, double y);
VT_EXPORT EncodedRegion makeSegment(cv::Point2d start, cv::Point2d end);
VT_EXPORT EncodedRegion makeRectangle(double left, double top, double width, double height, double angleDegrees = 0.0);
VT_EXPORT EncodedRegion makeCircle(cv::Point2d center, double radius);
VT_EXPORT EncodedRegion makePolygon(std::span<const cv::Point2d> points);
VT_EXPORT EncodedRegion makePointSet(std::span<const cv::Point2d> points);

// Constrained editing: the kind is fixed at construction, so a rectangle can never be written with
// circle values and no setter can change the encoded length. The wrapped vector is edited in place.
class RegionEditor
{
public:
    explicit RegionEditor(EncodedRegion &encoded);

    RoiKind kind() const noexcept;

    // Moves every coordinate of the region by the same delta. Which slots are coordinates is
    // decided here, not at call sites: a rectangle keeps storing its unrotated left/top corner.
    void translate(cv::Point2d delta);
    void moveTo(cv::Point2d value);
    void setSegment(cv::Point2d start, cv::Point2d end);
    // Point-like vertices only; a rectangle or circle has constrained handles instead because its
    // corners cannot move independently of width/height/radius.
    void setVertex(std::size_t index, cv::Point2d value);
    void setRectangle(double left, double top, double width, double height, double angleDegrees = 0.0);
    void setCircle(cv::Point2d center, double radius);
    void setPolygon(std::span<const cv::Point2d> points);
    void setPointSet(std::span<const cv::Point2d> points);

private:
    EncodedRegion &encoded_;
    RoiKind        kind_;
};

}
