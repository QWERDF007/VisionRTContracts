#pragma once

#include "visionrt/contracts/Array.h"
#include "visionrt/contracts/ROI.h"
#include "visionrt/contracts/TrainingSet.h"

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace visionrt {

struct Missing
{
    bool operator==(const Missing &) const = default;
};

using Image     = std::vector<cv::Mat>;
using PortValue = std::variant<Missing, std::int64_t, double, bool, std::string, DataRef<Image>, Array, DataRef<ROI>,
                              DataRef<TrainingSet>>;

VT_EXPORT bool isMissing(const PortValue& value) noexcept;
VT_EXPORT bool equalValues(const PortValue& left, const PortValue& right);
VT_EXPORT std::optional<TypeSpec> typeOf(const PortValue& value);
VT_EXPORT void validateValue(const TypeSpec& expected, const PortValue& value);

// The checks every image value must pass: two dimensions, a nonempty payload and a stride that
// stays inside the backing buffer. A trainer that receives raw pixels calls the same rule.
VT_EXPORT void validateImageFrame(const cv::Mat &image);
VT_EXPORT PortValue               retainValue(const PortValue &value, Owner dependency);

// The value a port carries when its declaration gives none: the type's own empty value (0, false,
// "", an empty batch or the empty three-layer ROI). It is a value, not a "no value" marker —
// Missing keeps meaning "this port has nothing", and a run's status says whether it succeeded.
VT_EXPORT PortValue defaultValue(const TypeSpec &type);

}
