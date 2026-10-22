#pragma once

#include "visionrt/contracts/Tool.h"
#include <optional>
#include <vector>

namespace visionrt {

struct PortDefinition;

// Execution-time constraint for one port: the same declaration used by the
// validator, generated wrappers and the host parameter boundary.
struct PortContract {
    std::string id;
    TypeSpec type;
    bool required = true;
    std::vector<PortValue> options;
    std::optional<PortValue> minimum;
    std::optional<PortValue> maximum;
};

VT_EXPORT PortContract portContractFromDefinition(const PortDefinition& port);

struct ToolContract {
    std::string toolId;
    std::string registryName;
    std::vector<PortContract> initParams;
    std::vector<PortContract> inputs;
    std::vector<PortContract> outputs;
};

VT_EXPORT void validateToolValues(const std::vector<PortContract>& contracts, const Values& values, bool allowMissing);

} // namespace visionrt
