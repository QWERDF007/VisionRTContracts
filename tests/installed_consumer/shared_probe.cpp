#include "shared_probe.h"

#include <visionrt/contracts/Value.h>

int contractSharedProbe() {
    return static_cast<int>(visionrt::dtypeSize(visionrt::DType::Float64));
}
