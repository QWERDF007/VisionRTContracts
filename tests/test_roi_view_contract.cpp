// Contract for the semantic ROI layer: the encoded vector is still the only storage, but every
// consumer reads named geometry instead of magic indices. Rectangle fields keep their original
// meaning - an unrotated left/top corner plus width, height and angle.
#include "visionrt/contracts/ROI.h"

#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace visionrt;

namespace {
bool near(double left, double right)
{
    return std::abs(left - right) < 1e-9;
}

template<class Callable>
void rejects(Callable &&callable)
{
    try
    {
        callable();
    }
    catch (const ContractError &)
    {
        return;
    }
    throw std::runtime_error("expected ContractError was not thrown");
}
} // namespace

int main()
{
    // Point
    const auto point = makePoint(3.5, -2.0);
    assert(point == (EncodedRegion{0, 3.5, -2.0}));
    const auto pointView = RegionView(point).point();
    assert(near(pointView.x(), 3.5) && near(pointView.y(), -2.0));

    // Segment
    const auto segment = makeSegment({1, 2}, {7, 9});
    assert(segment == (EncodedRegion{1, 1, 2, 7, 9}));
    const auto segmentView = RegionView(segment).segment();
    assert(near(segmentView.start().x, 1) && near(segmentView.start().y, 2));
    assert(near(segmentView.end().x, 7) && near(segmentView.end().y, 9));

    // Rectangle: stored as left/top/width/height/angle, centre is derived.
    const auto rectangle = makeRectangle(10, 20, 40, 8, 30);
    assert(rectangle == (EncodedRegion{2, 10, 20, 40, 8, 30}));
    const auto rectangleView = RegionView(rectangle).rectangle();
    assert(near(rectangleView.left(), 10) && near(rectangleView.top(), 20));
    assert(near(rectangleView.width(), 40) && near(rectangleView.height(), 8));
    assert(near(rectangleView.angle(), 30));
    assert(near(rectangleView.center().x, 30) && near(rectangleView.center().y, 24));
    const auto corners = rectangleView.corners();
    const auto legacy  = rectangleCorners(rectangle);
    for (std::size_t index = 0; index < corners.size(); ++index)
        assert(near(corners[index].x, legacy[index].x) && near(corners[index].y, legacy[index].y));
    // A zero angle keeps the axis-aligned box anchored at left/top.
    const auto axisAligned = RegionView(makeRectangle(0, 0, 4, 2)).rectangle().corners();
    assert(near(axisAligned[0].x, 0) && near(axisAligned[0].y, 0));
    assert(near(axisAligned[2].x, 4) && near(axisAligned[2].y, 2));

    // Circle
    const auto circle = makeCircle({5, 6}, 2.5);
    assert(circle == (EncodedRegion{3, 5, 6, 2.5}));
    const auto circleView = RegionView(circle).circle();
    assert(near(circleView.center().x, 5) && near(circleView.center().y, 6) && near(circleView.radius(), 2.5));

    // Polygon and point set share the pair layout but keep distinct meanings.
    const std::vector<cv::Point2d> polygonPoints{{0, 0}, {4, 0}, {4, 3}};
    const auto                     polygon = makePolygon(polygonPoints);
    assert(polygon.size() == 7 && polygon[0] == 4);
    const auto polygonView = RegionView(polygon).polygon();
    assert(polygonView.size() == 3 && near(polygonView.point(2).x, 4) && near(polygonView.point(2).y, 3));
    assert(polygonView.points().size() == 3);

    const std::vector<cv::Point2d> setPoints{{1, 1}, {2, 2}};
    const auto                     pointSet = makePointSet(setPoints);
    assert(pointSet.size() == 5 && pointSet[0] == 5);
    const auto pointSetView = RegionView(pointSet).pointSet();
    assert(pointSetView.size() == 2 && near(pointSetView.point(1).y, 2));

    // Kind is answered from one validation, and a mismatched accessor refuses instead of decoding
    // the wrong fields.
    assert(RegionView(rectangle).kind() == RoiKind::Rectangle);
    assert(RegionView::trusted(rectangle, RoiKind::Rectangle).kind() == RoiKind::Rectangle);
    rejects([&] { (void)RegionView(rectangle).circle(); });
    rejects([&] { (void)RegionView(point).pointSet(); });

    // Validation still refuses malformed encodings at the entry.
    rejects([&] { (void)RegionView(EncodedRegion{}); });
    rejects([&] { (void)RegionView(EncodedRegion{2, 0, 0, 0, 4, 0}); });
    rejects([&] { (void)RegionView(EncodedRegion{3, 0, 0, -1}); });
    rejects([&] { (void)RegionView(EncodedRegion{4, 0, 0, 1, 1}); });
    rejects([&] { (void)RegionView(EncodedRegion{9, 0, 0}); });
    rejects([&] { makeRectangle(0, 0, 0, 1); });
    rejects([&] { makeCircle({0, 0}, 0); });
    rejects([&] { makePolygon(std::vector<cv::Point2d>{{0, 0}, {1, 1}}); });
    rejects([&] { makePointSet(std::vector<cv::Point2d>{}); });

    // Constrained editing: the kind is fixed, lengths stay valid and the storage is edited in place.
    auto movable = makePoint(1, 2);
    {
        RegionEditor editor(movable);
        editor.moveTo({8, 9});
    }
    assert(movable == (EncodedRegion{0, 8, 9}));

    auto editable = makeRectangle(1, 2, 3, 4, 0);
    {
        RegionEditor editor(editable);
        editor.setRectangle(5, 6, 7, 8, 45);
        rejects([&] { editor.setCircle({0, 0}, 1); });
        rejects([&] { editor.setRectangle(0, 0, 0, 1); });
    }
    assert(editable == (EncodedRegion{2, 5, 6, 7, 8, 45}));

    auto editableSet = makePointSet(std::vector<cv::Point2d>{{1, 1}});
    {
        RegionEditor editor(editableSet);
        const std::vector<cv::Point2d> grown{{3, 3}, {4, 4}, {5, 5}};
        editor.setPointSet(grown);
    }
    assert(editableSet.size() == 7 && RegionView(editableSet).pointSet().size() == 3);

    // Translation moves exactly the coordinate slots; a rectangle stays a left/top rectangle and a
    // circle keeps its centre meaning instead of being reinterpreted as a corner.
    auto translatedRectangle = makeRectangle(10, 20, 40, 8, 30);
    RegionEditor(translatedRectangle).translate({-3, 4});
    assert(translatedRectangle == (EncodedRegion{2, 7, 24, 40, 8, 30}));
    auto translatedCircle = makeCircle({5, 6}, 2.5);
    RegionEditor(translatedCircle).translate({1, -2});
    assert(translatedCircle == (EncodedRegion{3, 6, 4, 2.5}));
    auto translatedSegment = makeSegment({1, 2}, {7, 9});
    RegionEditor(translatedSegment).translate({0.5, -0.5});
    assert(translatedSegment == (EncodedRegion{1, 1.5, 1.5, 7.5, 8.5}));
    const std::vector<cv::Point2d> translatedPoints{{0, 0}, {4, 0}, {4, 3}};
    auto                          translatedPolygon = makePolygon(translatedPoints);
    RegionEditor(translatedPolygon).translate({2, 1});
    assert(translatedPolygon == (EncodedRegion{4, 2, 1, 6, 1, 6, 4}));

    // Vertex editing is only offered where a vertex exists; rectangle and circle handles are
    // constrained setters instead.
    const std::vector<cv::Point2d> vertexPoints{{1, 1}, {2, 2}};
    auto                          vertexSet = makePointSet(vertexPoints);
    {
        RegionEditor editor(vertexSet);
        editor.setVertex(1, {9, 8});
        rejects([&] { editor.setVertex(2, {0, 0}); });
    }
    assert(vertexSet == (EncodedRegion{5, 1, 1, 9, 8}));
    auto vertexSegment = makeSegment({0, 0}, {1, 1});
    RegionEditor(vertexSegment).setVertex(0, {-2, -3});
    assert(vertexSegment == (EncodedRegion{1, -2, -3, 1, 1}));
    auto vertexRectangle = makeRectangle(0, 0, 4, 2);
    rejects([&] { RegionEditor(vertexRectangle).setVertex(0, {1, 1}); });

    // The transform path keeps its geometry while reading through the views.
    const std::array<double, 9> identity{1, 0, 0, 0, 1, 0, 0, 0, 1};
    const std::array<double, 9> translate{1, 0, 10, 0, 1, 20, 0, 0, 1};
    const auto                  originalRectangle = transformedRoiBounds(rectangle, identity);
    const auto                  shiftedRectangle  = transformedRoiBounds(rectangle, translate);
    assert(near(shiftedRectangle[0].x, originalRectangle[0].x + 10)
           && near(shiftedRectangle[0].y, originalRectangle[0].y + 20));
    assert(near(shiftedRectangle[1].x, originalRectangle[1].x + 10)
           && near(shiftedRectangle[1].y, originalRectangle[1].y + 20));
    const auto shiftedCircle = transformedRoiBounds(circle, translate);
    assert(near(shiftedCircle[0].x, 12.5) && near(shiftedCircle[0].y, 23.5));
    assert(near(shiftedCircle[1].x, 17.5) && near(shiftedCircle[1].y, 28.5));
    const auto shiftedPointSet = transformedRoiBounds(pointSet, translate);
    assert(near(shiftedPointSet[0].x, 11) && near(shiftedPointSet[1].y, 22));

    std::cout << "ROI semantic views: six kinds round-trip, rectangle keeps left/top, editing is kind-safe\n";
    return 0;
}
