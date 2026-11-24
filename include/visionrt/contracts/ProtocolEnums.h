#pragma once

namespace visionrt {
// A published training asset: typed records, parameters, lossless source frames and model files.
inline constexpr int kTrainingManifestVersion = 4;
#define VRT_ENUM_FILE "ProtocolEnums.def"
#include <visionrt/contracts/EnumDeclarations.h>

}
