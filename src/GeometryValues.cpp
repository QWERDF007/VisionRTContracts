#include "visionrt/contracts/GeometryValues.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace visionrt {

cv::Point2d DirectionView::vector() const noexcept
{
    const auto radians = angle() * std::numbers::pi / 180.0;
    return {length() * std::cos(radians), length() * std::sin(radians)};
}

void DirectionView::validate(std::span<const double> values)
{
    if (!std::all_of(values.begin(), values.end(), [](double value) { return std::isfinite(value); })
        || values[Length] < 0)
        throw ContractError("方向需要有限坐标、角度和非负长度");
}

void CoordinateFrameView::validate(std::span<const double> values)
{
    if (!std::all_of(values.begin(), values.end(), [](double value) { return std::isfinite(value); }))
        throw ContractError("二维坐标系需要有限数值");
    const auto determinant = values[AxisXdx] * values[AxisYdy] - values[AxisXdy] * values[AxisYdx];
    if (determinant == 0) throw ContractError("二维坐标系的两个轴必须线性无关");
}

std::vector<double> directionRecord(cv::Point2d start, double length, double angleDegrees)
{
    return {start.x, start.y, length, angleDegrees};
}

std::vector<double> coordinateFrameRecord(cv::Point2d origin, cv::Point2d xAxis, cv::Point2d yAxis)
{
    return {origin.x, origin.y, xAxis.x, xAxis.y, yAxis.x, yAxis.y};
}

} // namespace visionrt
