#pragma once

// A training call's input: one frame per distinct image plus any number of labelled records that
// name a frame. It is a plain tool-call value, so a native trainer receives real pixels and typed
// fields without a project path or an editor object. Images and records are keyed by their own
// numbers — a record never identifies its frame by list position.

#include "visionrt/contracts/Array.h"
#include "visionrt/contracts/DataRef.h"
#include "visionrt/contracts/Export.h"
#include "visionrt/contracts/ProtocolEnums.h"
#include "visionrt/contracts/ROI.h"

#include <cstdint>
#include <map>
#include <string>
#include <variant>

namespace visionrt {

using TrainingImageId  = std::uint32_t;
using TrainingRecordId = std::uint32_t;
// A record field is numbered the same way a port is: the declaration publishes the number and the
// record is looked up by it. It is not a PortId, because a field never becomes a port value.
using TrainingFieldId = std::uint32_t;

// A field holds any scalar a port can carry, an Array or a region group. Images and training sets
// are excluded by construction: nesting either one would put pixels or a second table inside a
// field and leave no single owner for them.
using TrainingFieldValue = std::variant<std::int64_t, double, bool, std::string, Array, DataRef<ROI>>;

struct TrainingImage
{
    TrainingImageId id = 0;
    // One frame with its own pixels; the colour order is metadata beside the pixels, never applied
    // to them. Two records that name this image share this one matrix.
    cv::Mat    pixels;
    ColorOrder colorOrder = ColorOrder::Automatic;
};

struct TrainingRecord
{
    TrainingRecordId                     id = 0;
    TrainingImageId                      imageId = 0;
    std::map<TrainingFieldId, TrainingFieldValue> fields;
};

struct TrainingSet
{
    std::map<TrainingImageId, TrainingImage>   images;
    std::map<TrainingRecordId, TrainingRecord> records;
};

// The frame a training image owns. The matrix is cloned, so releasing or replacing the upstream
// batch afterwards cannot change what the trainer reads.
VT_EXPORT TrainingImage trainingFrame(TrainingImageId id, const cv::Mat &frame, ColorOrder colorOrder);

// Every image is a nonempty two-dimensional matrix and every record names an image that exists.
// A set that fails here never reaches a trainer.
VT_EXPORT void validateTrainingSet(const TrainingSet &set);

} // namespace visionrt
