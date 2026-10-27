#pragma once

#include "visionrt/contracts/Array.h"
#include "visionrt/contracts/ROI.h"

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
using PortValue = std::variant<Missing, std::int64_t, double, bool, std::string, DataRef<Image>, Array, DataRef<ROI>>;

VT_EXPORT bool isMissing(const PortValue& value) noexcept;
VT_EXPORT bool equalValues(const PortValue& left, const PortValue& right);
VT_EXPORT std::optional<TypeSpec> typeOf(const PortValue& value);
VT_EXPORT void validateValue(const TypeSpec& expected, const PortValue& value);
VT_EXPORT PortValue               retainValue(const PortValue &value, Owner dependency);

}
