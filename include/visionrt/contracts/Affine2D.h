#pragma once

#include "visionrt/contracts/Array.h"
#include "visionrt/contracts/Export.h"
#include <opencv2/core/types.hpp>

#include <array>
#include <optional>
#include <span>
#include <vector>

namespace visionrt {

inline constexpr TypeSpec kAlignmentType{Type::Array, DType::Float64, 2};

// Reference to current image: x'=XX*x+XY*y+TX, y'=YX*x+YY*y+TY.
class VT_EXPORT Affine2D
{
public:
    enum Coefficient { XX, XY, TX, YX, YY, TY, CoefficientCount };
    using Coefficients = std::array<double, CoefficientCount>;
    Coefficients m{1, 0, 0, 0, 1, 0};

    constexpr Affine2D() = default;
    constexpr explicit Affine2D(Coefficients values) : m(values) {}
    static Affine2D identity() noexcept { return {}; }
    static std::optional<Affine2D> fromRows(std::span<const double> values) noexcept;
    bool isAffine() const noexcept;
    bool invertible() const noexcept;
    bool isSimilarity(double relativeTolerance = 1e-6) const noexcept;
    double scale() const noexcept;
    std::optional<Affine2D> inverse() const noexcept;
    Affine2D operator*(const Affine2D &other) const noexcept;
    cv::Point2d map(cv::Point2d point) const noexcept;
    void apply(double x, double y, double &outX, double &outY) const noexcept;
};

// Validates one N x 6 batch, then broadcasts a single transform or returns one per image.
VT_EXPORT std::vector<Affine2D> alignmentsFromArray(const Array &array, std::size_t imageCount);
VT_EXPORT Array alignmentArray(std::span<const Affine2D> values);
VT_EXPORT Array alignmentArray(const Affine2D &value);
VT_EXPORT Affine2D rotationAround(double centerX, double centerY, double clockwiseDegrees) noexcept;

}
