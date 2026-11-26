#include "visionrt/contracts/TrainingArchive.h"

#include "DeclarationNames.h"
#include "DeclarationValue.h"
#include "visionrt/contracts/Issue.h"
#include "visionrt/contracts/Value.h"
#include "visionrt/contracts/ZipArchive.h"

#include <opencv2/imgcodecs.hpp>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

#include <algorithm>
#include <limits>
#include <set>
#include <string>
#include <utility>

namespace visionrt {
namespace {

bool supportsPng(const cv::Mat &pixels) noexcept
{
    const int depth    = pixels.depth();
    const int channels = pixels.channels();
    return (depth == CV_8U || depth == CV_16U) && (channels == 1 || channels == 3 || channels == 4);
}

void validateArtifactId(std::string_view id)
{
    validateUtf8(id);
    if (id.empty())
        throw ContractError("训练模型标识不能为空");
    for (unsigned char c : id)
        if (c < 0x20 || c == 0x7f || c == '/' || c == '\\' || c == ':')
            throw ContractError("训练模型标识包含非法字符：" + std::string(id));
    if (id == "." || id == "..")
        throw ContractError("训练模型标识非法：" + std::string(id));
}

std::vector<std::uint8_t> encodeImageBytes(const cv::Mat &pixels, const std::string &extension)
{
    validateImageFrame(pixels);
    std::vector<unsigned char> encoded;
    if (!cv::imencode(extension, pixels, encoded) || encoded.empty())
        throw ContractError("训练图像编码失败（" + extension + "）");
    return {encoded.begin(), encoded.end()};
}

cv::Mat decodeImageBytes(const std::vector<std::uint8_t> &bytes, std::string_view entryName)
{
    if (bytes.empty())
        throw ContractError("训练图像数据为空：" + std::string(entryName));
    const cv::Mat raw(1, static_cast<int>(bytes.size()), CV_8U, const_cast<std::uint8_t *>(bytes.data()));
    cv::Mat       pixels = cv::imdecode(raw, cv::IMREAD_UNCHANGED);
    if (pixels.empty())
        throw ContractError("训练图像解码失败：" + std::string(entryName));
    validateImageFrame(pixels);
    return pixels;
}

const decljson::Node &requiredField(const decljson::Node &object, std::string_view key)
{
    const auto *found = object.find(key);
    if (!found)
        throw ContractError("训练资源清单缺少字段：" + std::string(key));
    return *found;
}

std::uint32_t readUint32(const decljson::Node &node, std::string_view what)
{
    const auto value = decljson::integer(node, what);
    if (value < 0 || value > std::numeric_limits<std::uint32_t>::max())
        throw ContractError(std::string(what) + " 超出 uint32 范围");
    return static_cast<std::uint32_t>(value);
}

std::string readString(const decljson::Node &node, std::string_view what)
{
    if (node.kind != decljson::Kind::String)
        throw ContractError(std::string(what) + " 必须是字符串");
    validateUtf8(node.text);
    return node.text;
}

decljson::Node parseManifest(const std::vector<std::uint8_t> &bytes)
{
    const std::string_view text(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    auto                   root = decljson::parse(text);
    if (root.kind != decljson::Kind::Object)
        throw ContractError("训练资源清单根节点必须是 JSON 对象");
    const auto version = decljson::integer(requiredField(root, "format_version"), "format_version");
    if (version != TrainingArchive::kFormatVersion)
        throw ContractError("不支持的训练资源包版本：" + std::to_string(version));
    return root;
}

} // namespace

void TrainingArchive::write(const std::filesystem::path                             &path,
                            const std::map<std::string, std::vector<std::uint8_t>> &models,
                            const std::vector<TrainingArchiveImage>                 &images)
{
    std::vector<ZipEntry> entries;
    rapidjson::StringBuffer                     buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    writer.SetIndent(' ', 2);

    writer.StartObject();
    writer.Key("format_version");
    writer.Int(kFormatVersion);

    writer.Key("models");
    writer.StartObject();
    for (const auto &[artifactId, bytes] : models)
    {
        validateArtifactId(artifactId);
        if (bytes.empty())
            throw ContractError("训练模型数据不能为空：" + artifactId);
        const std::string entryPath = "models/" + artifactId + ".bin";
        writer.Key(artifactId.c_str(), static_cast<rapidjson::SizeType>(artifactId.size()));
        writer.String(entryPath.c_str(), static_cast<rapidjson::SizeType>(entryPath.size()));
        entries.push_back({entryPath, bytes});
    }
    writer.EndObject();

    writer.Key("images");
    writer.StartArray();
    std::set<std::pair<PortId, TrainingImageId>> seenImages;
    for (const auto &image : images)
    {
        if (!seenImages.emplace(image.datasetPort, image.imageId).second)
            throw ContractError("训练资源包包含重复图像编号：" + std::to_string(image.datasetPort) + "/"
                                + std::to_string(image.imageId));
        const std::string ext       = supportsPng(image.pixels) ? ".png" : ".tiff";
        const std::string entryPath = "images/" + std::to_string(image.datasetPort) + "_"
                                    + std::to_string(image.imageId) + ext;
        entries.push_back({entryPath, encodeImageBytes(image.pixels, ext)});

        writer.StartObject();
        writer.Key("dataset_port");
        writer.Uint(image.datasetPort);
        writer.Key("image_id");
        writer.Uint(image.imageId);
        writer.Key("color_order");
        const char *order = decljson::wireName(image.colorOrder);
        writer.String(order);
        writer.Key("file");
        writer.String(entryPath.c_str(), static_cast<rapidjson::SizeType>(entryPath.size()));
        writer.EndObject();
    }
    writer.EndArray();
    writer.EndObject();

    std::vector<std::uint8_t> manifestBytes(buffer.GetString(), buffer.GetString() + buffer.GetSize());
    manifestBytes.push_back('\n');
    entries.insert(entries.begin(), ZipEntry{kManifestEntry, std::move(manifestBytes)});

    ZipArchive::write(path, entries);
}

std::vector<std::uint8_t> TrainingArchive::readModel(const std::filesystem::path &path, std::string_view artifactId)
{
    validateArtifactId(artifactId);
    const auto  manifestBytes = ZipArchive::readEntry(path, kManifestEntry);
    const auto  root          = parseManifest(manifestBytes);
    const auto &models        = requiredField(root, "models");
    if (models.kind != decljson::Kind::Object)
        throw ContractError("训练资源清单 models 必须是对象");
    const auto *entryNode = models.find(artifactId);
    if (!entryNode)
        throw ContractError("训练资源包不包含模型：" + std::string(artifactId));
    const auto entryPath = readString(*entryNode, "models." + std::string(artifactId));
    auto       data      = ZipArchive::readEntry(path, entryPath);
    if (data.empty())
        throw ContractError("训练资源包缺少模型数据：" + entryPath);
    return data;
}

std::vector<TrainingArchiveImage> TrainingArchive::readImages(const std::filesystem::path &path)
{
    const auto entries  = ZipArchive::readAll(path);
    const auto foundMan = entries.find(kManifestEntry);
    if (foundMan == entries.end())
        throw ContractError("训练资源包缺少 manifest.json");
    const auto  root   = parseManifest(foundMan->second);
    const auto &images = requiredField(root, "images");
    if (images.kind != decljson::Kind::Array)
        throw ContractError("训练资源清单 images 必须是数组");

    std::vector<TrainingArchiveImage>            result;
    std::set<std::pair<PortId, TrainingImageId>> seen;
    result.reserve(images.items.size());
    for (const auto &item : images.items)
    {
        if (item.kind != decljson::Kind::Object)
            throw ContractError("训练资源清单图像条目必须是对象");
        TrainingArchiveImage entry;
        entry.datasetPort = readUint32(requiredField(item, "dataset_port"), "dataset_port");
        entry.imageId     = readUint32(requiredField(item, "image_id"), "image_id");
        entry.colorOrder  = decljson::enumeration<ColorOrder>(requiredField(item, "color_order"), "color_order");
        const auto file   = readString(requiredField(item, "file"), "file");
        if (!seen.emplace(entry.datasetPort, entry.imageId).second)
            throw ContractError("训练资源清单包含重复图像条目");
        const auto found = entries.find(file);
        if (found == entries.end())
            throw ContractError("训练资源包缺少图像文件：" + file);
        entry.pixels = decodeImageBytes(found->second, file);
        result.push_back(std::move(entry));
    }
    return result;
}

} // namespace visionrt
