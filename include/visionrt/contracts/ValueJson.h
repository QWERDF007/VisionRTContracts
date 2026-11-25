#pragma once

#include "visionrt/contracts/Export.h"
#include "visionrt/contracts/Value.h"

#include <string>
#include <string_view>
#include <memory>
#include <span>

namespace visionrt {

// Portable typed values used by declarations, project literals and training record fields.
// Pixel storage uses OpenCV's binary JSON matrix representation and preserves depth and channels.
VT_EXPORT PortValue readValueJson(std::string_view json, const TypeSpec &type);
VT_EXPORT std::string writeValueJson(const PortValue &value, const TypeSpec &type);

class VT_EXPORT ValueJsonDocument
{
public:
    explicit ValueJsonDocument(std::string_view json);
    PortValue read(std::span<const std::string> path, const TypeSpec &type) const;
private:
    struct Impl;
    std::shared_ptr<const Impl> document_;
};

}
