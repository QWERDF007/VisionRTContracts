#pragma once

#include "visionrt/contracts/Array.h"
#include "visionrt/contracts/GeometryValues.h"
#include "visionrt/contracts/ROI.h"
#include "visionrt/contracts/ShapeValues.h"

#include <string>
#include <variant>
#include <vector>

namespace visionrt {
struct TrainingSet;
struct Missing { bool operator==(const Missing &) const = default; };
using Image = std::vector<cv::Mat>;
using PortValue = std::variant<Missing, std::int64_t, double, bool, std::string, DataRef<Image>, Array,
                              DataRef<TrainingSet>, CoordinateFrame2D, Direction, Point, Line, Rectangle, Circle, Polygon>;
} // namespace visionrt
