#pragma once

#include "visionrt/contracts/Export.h"

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace visionrt {

struct ZipEntry
{
    std::string               name;
    std::vector<std::uint8_t> bytes;
};

class VT_EXPORT ZipArchive
{
public:
    static std::map<std::string, std::vector<std::uint8_t>> readAll(const std::filesystem::path &path);
    static std::vector<std::uint8_t>                        readEntry(const std::filesystem::path &path,
                                                                      std::string_view             entryName);
    static void write(const std::filesystem::path &path, const std::vector<ZipEntry> &entries);
};

} // namespace visionrt
