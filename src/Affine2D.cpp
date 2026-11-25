#include "visionrt/contracts/Affine2D.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace visionrt {

std::optional<Affine2D> Affine2D::fromRows(std::span<const double> values) noexcept
{
    if (values.size() != CoefficientCount) return std::nullopt;
    Affine2D result;
    std::copy(values.begin(), values.end(), result.m.begin());
    return result.isAffine() ? std::optional{result} : std::nullopt;
}
bool Affine2D::isAffine() const noexcept
{
    return std::ranges::all_of(m, [](double value) { return std::isfinite(value); });
}
bool Affine2D::invertible() const noexcept
{
    const auto determinant = m[XX] * m[YY] - m[XY] * m[YX];
    return std::isfinite(determinant) && std::abs(determinant) > 1e-12;
}
bool Affine2D::isSimilarity(double relativeTolerance) const noexcept
{
    if (!isAffine() || !invertible()) return false;
    const auto a = m[XX], b = m[XY], c = m[YX], d = m[YY];
    const auto squared = a*a + b*b;
    const auto tolerance = relativeTolerance * std::max(1.0, squared);
    return std::abs(c*c + d*d - squared) <= tolerance && std::abs(a*c + b*d) <= tolerance && a*d - b*c > 0;
}
double Affine2D::scale() const noexcept { return std::sqrt(std::abs(m[XX]*m[YY] - m[XY]*m[YX])); }
std::optional<Affine2D> Affine2D::inverse() const noexcept
{
    if (!isAffine() || !invertible()) return std::nullopt;
    const auto determinant = m[XX]*m[YY] - m[XY]*m[YX];
    const auto a = m[YY]/determinant, b = -m[XY]/determinant, c = -m[YX]/determinant, d = m[XX]/determinant;
    return Affine2D{{a, b, -a*m[TX]-b*m[TY], c, d, -c*m[TX]-d*m[TY]}};
}
Affine2D Affine2D::operator*(const Affine2D &other) const noexcept
{
    const auto &n = other.m;
    return Affine2D{{m[XX]*n[XX]+m[XY]*n[YX], m[XX]*n[XY]+m[XY]*n[YY], m[XX]*n[TX]+m[XY]*n[TY]+m[TX],
                      m[YX]*n[XX]+m[YY]*n[YX], m[YX]*n[XY]+m[YY]*n[YY], m[YX]*n[TX]+m[YY]*n[TY]+m[TY]}};
}
cv::Point2d Affine2D::map(cv::Point2d point) const noexcept
{
    return {m[XX]*point.x + m[XY]*point.y + m[TX], m[YX]*point.x + m[YY]*point.y + m[TY]};
}
void Affine2D::apply(double x, double y, double &outX, double &outY) const noexcept
{
    const auto result = map({x, y}); outX = result.x; outY = result.y;
}
std::vector<Affine2D> alignmentsFromArray(const Array &array, std::size_t imageCount)
{
    if (array.type() != kAlignmentType) throw ContractError("对齐数据必须是二维浮点数数组");
    std::size_t count;
    if (array.layout() == ArrayLayout::Dense)
    {
        if (array.shape()[1] != Affine2D::CoefficientCount) throw ContractError("对齐数据每项必须包含六个系数");
        count = static_cast<std::size_t>(array.shape()[0]);
    }
    else
    {
        const auto &bounds = array.offsets().front();
        count = bounds.size() - 1;
        for (std::size_t i = 1; i < bounds.size(); ++i)
            if (bounds[i] - bounds[i-1] != Affine2D::CoefficientCount) throw ContractError("对齐数据每项必须包含六个系数");
    }
    if (count != 1 && count != imageCount) throw ContractError("对齐数量必须为一或与图像数量一致");
    std::vector<Affine2D> result;
    const auto elements = array.elements<double>();
    for (std::size_t i = 0; i < count; ++i)
    {
        auto value = Affine2D::fromRows(elements.subspan(i * Affine2D::CoefficientCount, Affine2D::CoefficientCount));
        if (!value || !value->invertible()) throw ContractError("对齐数据必须是有限可逆仿射变换");
        result.push_back(*value);
    }
    if (count == 1 && imageCount > 1) result.resize(imageCount, result.front());
    return result;
}
Array alignmentArray(std::span<const Affine2D> values)
{
    std::vector<double> data;
    data.reserve(values.size() * Affine2D::CoefficientCount);
    for (const auto &value : values) data.insert(data.end(), value.m.begin(), value.m.end());
    return Array::dense(DType::Float64, {static_cast<std::int64_t>(values.size()), Affine2D::CoefficientCount}, std::move(data));
}
Array alignmentArray(const Affine2D &value) { return alignmentArray(std::span(&value, 1)); }
Affine2D rotationAround(double x, double y, double clockwiseDegrees) noexcept
{
    const double radians = clockwiseDegrees * std::numbers::pi / 180.0;
    const double c = std::cos(radians), s = std::sin(radians);
    return Affine2D{{c, -s, x-c*x+s*y, s, c, y-s*x-c*y}};
}

}
