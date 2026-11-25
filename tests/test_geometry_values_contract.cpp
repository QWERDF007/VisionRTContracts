#include <visionrt/contracts/GeometryValues.h>
#include <visionrt/contracts/Affine2D.h>
#include <visionrt/contracts/Value.h>
#include <visionrt/contracts/ValueJson.h>
#include <visionrt/contracts/Parameters.h>

#include <cassert>
#include <cmath>
#include <iostream>

using namespace visionrt;

int main()
{
    static_assert(!std::is_constructible_v<PortValue, DataRef<ROI>>);
    const auto circles = Circle::fromNested({{{10, 20, 3}, {40, 50, 5}}, {}, {{2, 4, 1}}});
    assert(circles.images() == 3 && circles.count(0) == 2 && circles.count(1) == 0);
    assert(circles.at(0, 0).center() == cv::Point2d(10, 20));
    assert(circles.at(0, 0).radius() == 3);
    assert(typeOf(PortValue{circles}) == TypeSpec{Type::Circle});
    assert(!compatible({Type::Circle}, {Type::Rectangle}));
    assert(!compatible({Type::Point}, {Type::Line}));
    const auto rectangles = Rectangle::fromNested({{{1, 2, 30, 40, 15}}});
    assert(rectangles.at(0, 0).width() == 30);
    const auto lines = Line::fromNested({{{1, 2, 3, 4}}});
    assert(lines.at(0, 0).end() == cv::Point2d(3, 4));
    const auto points = Point::fromNested({{{1, 2}, {3, 4}}});
    assert(points.at(0, 1).point(0) == cv::Point2d(3, 4));
    const auto polygons = Polygon::fromNested({{{0, 0, 4, 0, 4, 3}}});
    assert(polygons.at(0, 0).size() == 3);
    bool wrongShape = false;
    try { Circle::fromEncoded(ROI{{makeRectangle(0, 0, 4, 3)}}); }
    catch (const ContractError &) { wrongShape = true; }
    assert(wrongShape);
    const auto restored = std::get<Circle>(readValueJson(writeValueJson(circles, {Type::Circle}), {Type::Circle}));
    assert(restored == circles && restored.nested()[0][0] == std::vector<double>({10, 20, 3}));
    const Values typedInputs{{1, circles}, {2, rectangles}};
    const InputReader typedReader(typedInputs);
    assert(typedReader.circle(1) && !typedReader.circle(2));
    OutputWriter typedWriter;
    assert(!typedWriter.circle(3, circles));
    assert(typeOf(typedWriter.take().at(3)) == TypeSpec{Type::Circle});
    const Affine2D translated{{1, 0, 10, 0, 1, 20}};
    const auto alignment = alignmentArray(translated);
    assert(alignment.rank() == 2 && alignment.shape()[0] == 1 && alignment.shape()[1] == Affine2D::CoefficientCount);
    const auto transforms = alignmentsFromArray(alignment, 3);
    assert(transforms.size() == 3 && transforms[2].map({2, 3}) == cv::Point2d(12, 23));
    assert(translated.inverse()->map({12, 23}) == cv::Point2d(2, 3));
    const auto directions = Direction::fromNested({{{10, 20, 5, 90}, {2, 3, 4, 0}}, {}, {{0, 0, 2, 180}}});
    assert(directions.images() == 3 && directions.count(0) == 2 && directions.count(1) == 0);
    const auto first = directions.at(0, 0);
    assert(first.start() == cv::Point2d(10, 20));
    assert(first.length() == 5 && first.angle() == 90);
    assert(cv::norm(first.end() - cv::Point2d(10, 25)) < 1e-12);
    assert(directions.at(0, 1).end() == cv::Point2d(6, 3));
    assert(typeOf(PortValue{directions}) == TypeSpec{Type::Direction});
    validateValue({Type::Direction}, directions);
    assert(equalValues(directions, Direction(directions.array().clone())));

    const auto frames = CoordinateFrame2D::fromNested({{{3, 4, 2, 1, -1, 3}}});
    const auto frame = frames.at(0, 0);
    assert(frame.origin() == cv::Point2d(3, 4));
    assert(frame.xEnd() == cv::Point2d(5, 5));
    assert(frame.yEnd() == cv::Point2d(2, 7));
    assert(typeOf(PortValue{frames}) == TypeSpec{Type::CoordinateFrame2D});
    validateValue({Type::CoordinateFrame2D}, frames);
    assert(std::get<Direction>(defaultValue({Type::Direction})).images() == 0);
    assert(std::get<CoordinateFrame2D>(defaultValue({Type::CoordinateFrame2D})).images() == 0);
    std::cout << "geometry values passed\n";
}
