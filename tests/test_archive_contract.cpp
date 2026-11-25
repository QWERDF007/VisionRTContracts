#include "visionrt/contracts/TrainingArchive.h"
#include "visionrt/contracts/ZipArchive.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace visionrt;

namespace {

void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

template<class Action>
void rejects(Action action, const std::string &label)
{
    bool threw = false;
    try
    {
        action();
    }
    catch (const ContractError &)
    {
        threw = true;
    }
    require(threw, "expected ContractError: " + label);
}

} // namespace

int main()
try
{
    const auto tempDir = std::filesystem::temp_directory_path() / u8"visionrt_contracts_archive_test_中文目录";
    std::filesystem::remove_all(tempDir);
    std::filesystem::create_directories(tempDir);

    // 1. ZipArchive round-trip with UTF-8 Chinese entry names and paths
    const auto zipPath = tempDir / u8"测试容器.vrtapp";
    const std::string payload = R"({"format_version":9,"name":"中文工程"})";
    ZipArchive::write(zipPath, {
        {"program.json", {payload.begin(), payload.end()}},
        {std::string(reinterpret_cast<const char *>(u8"子目录/说明.txt")), {'o', 'k'}},
    });
    require(std::filesystem::is_regular_file(zipPath), "zip file created");
    const auto programBytes = ZipArchive::readEntry(zipPath, "program.json");
    require(std::string(programBytes.begin(), programBytes.end()) == payload, "program.json round-trip");
    const auto all = ZipArchive::readAll(zipPath);
    require(all.size() == 2 && all.at(std::string(reinterpret_cast<const char *>(u8"子目录/说明.txt"))) == std::vector<std::uint8_t>({'o', 'k'}),
            "utf8 entry name round-trip");

    // 2. Reject invalid paths, duplicates, missing entries, and corrupt archives
    rejects([&] { ZipArchive::write(tempDir / "bad1.zip", {{"../escape.txt", {'x'}}}); }, "path traversal");
    rejects([&] { ZipArchive::write(tempDir / "bad2.zip", {{"/abs.txt", {'x'}}}); }, "absolute entry");
    rejects([&] { ZipArchive::write(tempDir / "bad3.zip", {{"a\\b.txt", {'x'}}}); }, "backslash entry");
    rejects([&] { ZipArchive::write(tempDir / "bad4.zip", {{"dup.txt", {'a'}}, {"dup.txt", {'b'}}}); }, "duplicate entry");
    rejects([&] { ZipArchive::readEntry(zipPath, "missing.json"); }, "missing entry");

    const auto corrupt = tempDir / "corrupt.vrtapp";
    {
        std::ofstream out(corrupt, std::ios::binary);
        out << "not a zip file";
    }
    rejects([&] { ZipArchive::readAll(corrupt); }, "corrupt archive");

    // 3. TrainingArchive round-trip: model bytes + 8U PNG + 16U PNG + 32F TIFF
    const auto bundlePath = tempDir / u8"模型与训练图.zip";
    cv::Mat    bgr8(4, 6, CV_8UC3, cv::Scalar(12, 34, 56));
    cv::Mat    gray16(3, 5, CV_16UC1, cv::Scalar(4321));
    cv::Mat    float32(2, 4, CV_32FC1, cv::Scalar(1.25f));
    float32.at<float>(1, 3) = -7.5f;

    const std::vector<std::uint8_t> modelBytes = {0x01, 0x02, 0x03, 0xAB, 0xCD};
    TrainingArchive::write(bundlePath, {{"calibration-model", modelBytes}}, {
        {0, 1, ColorOrder::BGR, bgr8},
        {0, 2, ColorOrder::Gray, gray16},
        {2, 9, ColorOrder::Automatic, float32},
    });

    const auto loadedModel = TrainingArchive::readModel(bundlePath, "calibration-model");
    require(loadedModel == modelBytes, "model bytes exact round-trip");
    rejects([&] { TrainingArchive::readModel(bundlePath, "nonexistent"); }, "missing model artifact");

    const auto loadedImages = TrainingArchive::readImages(bundlePath);
    require(loadedImages.size() == 3, "all three training images restored");
    require(loadedImages[0].datasetPort == 0 && loadedImages[0].imageId == 1
                && loadedImages[0].colorOrder == ColorOrder::BGR
                && loadedImages[0].pixels.type() == CV_8UC3
                && loadedImages[0].pixels.at<cv::Vec3b>(2, 3) == cv::Vec3b(12, 34, 56),
            "8-bit PNG lossless round-trip");
    require(loadedImages[1].datasetPort == 0 && loadedImages[1].imageId == 2
                && loadedImages[1].colorOrder == ColorOrder::Gray
                && loadedImages[1].pixels.type() == CV_16UC1
                && loadedImages[1].pixels.at<std::uint16_t>(1, 2) == 4321,
            "16-bit PNG lossless round-trip");
    require(loadedImages[2].datasetPort == 2 && loadedImages[2].imageId == 9
                && loadedImages[2].pixels.type() == CV_32FC1
                && loadedImages[2].pixels.at<float>(1, 3) == -7.5f,
            "32-bit float TIFF lossless round-trip");

    std::filesystem::remove_all(tempDir);
    std::cout << "ZipArchive and TrainingArchive contracts verified\n";
    return 0;
}
catch (const std::exception &error)
{
    std::cerr << error.what() << '\n';
    return 1;
}
