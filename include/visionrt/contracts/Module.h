#pragma once

#include "visionrt/contracts/Export.h"
#include "visionrt/contracts/Registry.h"
#include <cstdint>

namespace visionrt {

constexpr std::uint32_t MODULE_ABI_MAJOR = 4;

struct ModuleApi {
    std::uint32_t abi_major;
    std::uint32_t struct_bytes;
    ToolStatus (VT_CALL *register_tools)(IToolRegistry&) noexcept;
};

} // namespace visionrt

extern "C" VT_EXPORT const visionrt::ModuleApi* VT_CALL visiontoolsModuleApi() noexcept;
