#pragma once

#include "visionrt/contracts/Export.h"
#include "visionrt/contracts/Tool.h"

namespace visionrt {

struct ToolFactoryDescriptor {
    const char* tool_id;
    IToolInstance* (VT_CALL *create)(const ToolServices*, ToolStatus*) noexcept;
    void (VT_CALL *destroy)(IToolInstance*) noexcept;
};

class IToolRegistry {
public:
    virtual ~IToolRegistry() = default;
    virtual ToolStatus add(const ToolFactoryDescriptor& descriptor) noexcept = 0;
};

} // namespace visionrt
