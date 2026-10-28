#pragma once

#include "visionrt/contracts/Export.h"
#include "visionrt/contracts/Registry.h"
#include <cstdint>

namespace visionrt {

// 7: ToolResult carries outputs (including __status_code and __status_message) and detail.
constexpr std::uint32_t MODULE_ABI_MAJOR = 7;

struct ModuleApi {
    std::uint32_t abi_major;
    std::uint32_t struct_bytes;
    ToolStatus (VT_CALL *register_tools)(IToolRegistry&) noexcept;
};

} // namespace visionrt

extern "C" VT_EXPORT const visionrt::ModuleApi* VT_CALL visiontoolsModuleApi() noexcept;
