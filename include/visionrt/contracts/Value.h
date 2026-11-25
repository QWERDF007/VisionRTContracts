#pragma once

#include "visionrt/contracts/ValueTypes.h"
#include "visionrt/contracts/TrainingSet.h"

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace visionrt {

VT_EXPORT bool isMissing(const PortValue& value) noexcept;
VT_EXPORT bool equalValues(const PortValue& left, const PortValue& right);
VT_EXPORT std::optional<TypeSpec> typeOf(const PortValue& value);
VT_EXPORT void validateValue(const TypeSpec& expected, const PortValue& value);

// Encoded regions are an internal view for shared drawing and algorithms. Every public value
// retains its concrete shape type; constructing a value checks all records against that type.
VT_EXPORT const ROI *shapeRegions(const PortValue &value) noexcept;
VT_EXPORT PortValue shapeValue(Type type, ROI encoded);

// The checks every image value must pass: two dimensions, a nonempty payload and a stride that
// stays inside the backing buffer. A trainer that receives raw pixels calls the same rule.
VT_EXPORT void validateImageFrame(const cv::Mat &image);
VT_EXPORT PortValue               retainValue(const PortValue &value, Owner dependency);

// The value a port carries when its declaration gives none: the type's own empty value (0, false,
// "" or an empty batch). It is a value, not a "no value" marker —
// Missing keeps meaning "this port has nothing", and a run's status says whether it succeeded.
VT_EXPORT PortValue defaultValue(const TypeSpec &type);

}
