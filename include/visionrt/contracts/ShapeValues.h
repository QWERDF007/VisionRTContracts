#pragma once

#include "visionrt/contracts/DataRef.h"
#include "visionrt/contracts/ROI.h"

#include <optional>
#include <type_traits>

namespace visionrt {

constexpr std::optional<RoiKind> shapeKind(Type type) noexcept
{
    switch (type)
    {
#define VRT_SHAPE(name, kind, view, code, wire, label) case Type::name: return RoiKind::kind;
#include "ShapeKinds.def"
#undef VRT_SHAPE
    default: return std::nullopt;
    }
}

template<RoiKind Kind, class View, Type ValueType>
class ShapeBatch
{
public:
    static constexpr RoiKind kind = Kind;
    static constexpr Type valueType = ValueType;
    using Nested = ROI;

    ShapeBatch() : value_(ROI{}) {}

    static ShapeBatch fromNested(Nested coordinates)
    {
        for (auto &image : coordinates)
            for (auto &record : image)
                record.insert(record.begin(), static_cast<double>(Kind));
        return fromEncoded(std::move(coordinates));
    }

    static ShapeBatch fromEncoded(ROI encoded)
    {
        for (const auto &image : encoded)
            for (const auto &record : image)
            {
                validateRoi(record);
                if (roiKind(record) != Kind)
                    throw ContractError(std::string(typeLabel(ValueType)) + "参数不能包含其他形状");
            }
        return ShapeBatch(DataRef<ROI>(std::move(encoded)));
    }

    TypeSpec type() const noexcept { return {ValueType}; }
    const ROI &get() const noexcept { return value_.get(); }
    Nested nested() const
    {
        auto coordinates = get();
        for (auto &image : coordinates)
            for (auto &record : image) record.erase(record.begin());
        return coordinates;
    }
    std::size_t images() const noexcept { return get().size(); }
    std::size_t count(std::size_t image) const { return get().at(image).size(); }
    View at(std::size_t image, std::size_t item) const { return View(get().at(image).at(item)); }
    ShapeBatch clone() const { return ShapeBatch(DataRef<ROI>(get())); }
    ShapeBatch retainedBy(Owner owner) const { return ShapeBatch(value_.retainedBy(std::move(owner))); }
    bool operator==(const ShapeBatch &other) const { return get() == other.get(); }

private:
    explicit ShapeBatch(DataRef<ROI> value) : value_(std::move(value)) {}
    DataRef<ROI> value_;
};

#define VRT_SHAPE(name, kind, view, code, wire, label) using name = ShapeBatch<RoiKind::kind, view, Type::name>;
#include "ShapeKinds.def"
#undef VRT_SHAPE

template<class T> inline constexpr bool isShapeValue = false;
template<RoiKind Kind, class View, Type ValueType>
inline constexpr bool isShapeValue<ShapeBatch<Kind, View, ValueType>> = true;

} // namespace visionrt
