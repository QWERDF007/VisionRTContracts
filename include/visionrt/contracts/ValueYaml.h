#pragma once

#include "visionrt/contracts/Value.h"
#include <yaml-cpp/yaml.h>

namespace visionrt {

VT_EXPORT const char* typeName(Type type);
VT_EXPORT const char* dtypeName(DType dtype);
VT_EXPORT TypeSpec decodeTypeSpec(const YAML::Node& node);
VT_EXPORT YAML::Node encodeTypeSpec(const TypeSpec& type);
VT_EXPORT PortValue decodeValue(const YAML::Node& node, const TypeSpec& type);
VT_EXPORT YAML::Node encodeValue(const PortValue& value, const TypeSpec& type);

} // namespace visionrt
