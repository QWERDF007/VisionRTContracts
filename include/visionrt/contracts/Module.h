#pragma once

#include "visionrt/contracts/Export.h"
#include "visionrt/contracts/Registry.h"
#include <cstdint>

namespace visionrt {

// 9: parameter identity moved from port names to numeric PortId. Values are keyed by the number the
//    declaration publishes; the upstream status is input 0 and the status envelope is outputs 0/1.
// 8: Array owns one flat dtype buffer with a Dense shape or Ragged offsets, and exposes spans
//    instead of a nested-vector variant. ToolResult keeps carrying outputs and detail.
constexpr std::uint32_t MODULE_ABI_MAJOR = 9;

struct ModuleApi {
    std::uint32_t abi_major;
    std::uint32_t struct_bytes;
    ToolStatus (VT_CALL *register_tools)(IToolRegistry&) noexcept;
};

} // namespace visionrt

extern "C" VT_EXPORT const visionrt::ModuleApi* VT_CALL visiontoolsModuleApi() noexcept;
