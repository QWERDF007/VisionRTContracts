#pragma once

// Value and type codecs for tool declarations. A declaration carries typed defaults, constraints
// and step sizes; this is the one place that turns those into JSON and back for the declaration
// document. Project files and the editor keep their own value codec, because their stored shape
// carries layout details a declaration never states.

#include "JsonDocument.h"

#include "visionrt/contracts/TypeSpec.h"
#include "visionrt/contracts/Value.h"

#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

namespace visionrt::decljson {

using Writer = rapidjson::PrettyWriter<rapidjson::StringBuffer>;

TypeSpec decodeType(const Node &node);
void     encodeType(Writer &writer, const TypeSpec &type);

PortValue decodeValue(const Node &node, const TypeSpec &type);
void      encodeValue(Writer &writer, const PortValue &value, const TypeSpec &type);

// A single JSON number as the exact integer its text spells. `1.0` is rejected here: the field
// declares an integer and a decimal point is a different kind of number, not a formatting choice.
std::int64_t integer(const Node &node, std::string_view what);

} // namespace visionrt::decljson
