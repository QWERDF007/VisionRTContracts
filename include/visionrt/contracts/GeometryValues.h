#pragma once

#include "visionrt/contracts/Array.h"

#include <opencv2/core.hpp>
#include <span>

namespace visionrt {

template<class Record> class GeometryBatch;

class DirectionView
{
public:
    static constexpr Type valueType = Type::Direction;
    enum Field : std::size_t { StartX, StartY, Length, Angle, FieldCount };
    cv::Point2d start() const noexcept { return {values_[StartX], values_[StartY]}; }
    double length() const noexcept { return values_[Length]; }
    double angle() const noexcept { return values_[Angle]; }
    VT_EXPORT cv::Point2d vector() const noexcept;
    cv::Point2d end() const noexcept { return start() + vector(); }
    VT_EXPORT static void validate(std::span<const double> values);
private:
    template<class> friend class GeometryBatch;
    explicit DirectionView(std::span<const double> values) : values_(values) {}
    std::span<const double> values_;
};

class CoordinateFrameView
{
public:
    static constexpr Type valueType = Type::CoordinateFrame2D;
    enum Field : std::size_t { OriginX, OriginY, AxisXdx, AxisXdy, AxisYdx, AxisYdy, FieldCount };
    cv::Point2d origin() const noexcept { return {values_[OriginX], values_[OriginY]}; }
    cv::Point2d xAxis() const noexcept { return {values_[AxisXdx], values_[AxisXdy]}; }
    cv::Point2d yAxis() const noexcept { return {values_[AxisYdx], values_[AxisYdy]}; }
    cv::Point2d xEnd() const noexcept { return origin() + xAxis(); }
    cv::Point2d yEnd() const noexcept { return origin() + yAxis(); }
    VT_EXPORT static void validate(std::span<const double> values);
private:
    template<class> friend class GeometryBatch;
    explicit CoordinateFrameView(std::span<const double> values) : values_(values) {}
    std::span<const double> values_;
};

// Image, item, fields. Both geometries reuse Array storage, including irregular item counts.
// Their distinct wrappers preserve semantic types in PortValue without copying the leaf buffer.
template<class Record>
class GeometryBatch
{
public:
    using Nested = NestedVector<double, 3>;
    GeometryBatch() : values_(Array::dense(DType::Float64, {0, 0, Record::FieldCount}, std::vector<double>{})) {}
    explicit GeometryBatch(Array values) : values_(std::move(values))
    {
        if (values_.type() != storageType())
            throw ContractError("几何数据需要三维双精度数组");
        if (values_.layout() == ArrayLayout::Dense)
        {
            if (values_.elementCount() == 0 && values_.shape().back() == 0)
                values_ = Array::dense(DType::Float64, {values_.shape().front(), values_.shape().at(1), Record::FieldCount}, std::vector<double>{});
            if (values_.shape().back() != Record::FieldCount)
                throw ContractError("几何记录字段数量不匹配");
        }
        else
        {
            const auto &fields = values_.offsets().back();
            for (std::size_t i = 1; i < fields.size(); ++i)
                if (fields[i] - fields[i - 1] != Record::FieldCount)
                    throw ContractError("几何记录字段数量不匹配");
        }
        const auto data = values_.template elements<double>();
        for (std::size_t offset = 0; offset < data.size(); offset += Record::FieldCount)
            Record::validate(data.subspan(offset, Record::FieldCount));
    }
    static GeometryBatch fromNested(Nested values) { return GeometryBatch(Array::fromNested(std::move(values))); }
    static TypeSpec storageType() noexcept { return {Type::Array, DType::Float64, 3}; }
    TypeSpec type() const noexcept { return {Record::valueType}; }
    const Array &array() const noexcept { return values_; }
    GeometryBatch clone() const { return GeometryBatch(values_.clone()); }
    GeometryBatch retainedBy(Owner owner) const { auto result = *this; result.values_ = values_.retainedBy(std::move(owner)); return result; }
    std::size_t images() const noexcept
    {
        return values_.layout() == ArrayLayout::Dense ? static_cast<std::size_t>(values_.shape().front())
            : values_.offsets().front().size() - 1;
    }
    std::size_t count(std::size_t image) const
    {
        if (values_.layout() == ArrayLayout::Dense) return static_cast<std::size_t>(values_.shape().at(1));
        const auto &offsets = values_.offsets().front();
        return static_cast<std::size_t>(offsets.at(image + 1) - offsets.at(image));
    }
    Record at(std::size_t image, std::size_t item) const
    {
        const auto record = values_.layout() == ArrayLayout::Dense ? image * count(image) + item
            : static_cast<std::size_t>(values_.offsets().front().at(image)) + item;
        return Record(values_.template elements<double>().subspan(record * Record::FieldCount, Record::FieldCount));
    }
    bool operator==(const GeometryBatch &other) const { return values_ == other.values_; }
private:
    Array values_;
};

using Direction = GeometryBatch<DirectionView>;
using CoordinateFrame2D = GeometryBatch<CoordinateFrameView>;

VT_EXPORT std::vector<double> directionRecord(cv::Point2d start, double length, double angleDegrees);
VT_EXPORT std::vector<double> coordinateFrameRecord(cv::Point2d origin, cv::Point2d xAxis, cv::Point2d yAxis);

} // namespace visionrt
