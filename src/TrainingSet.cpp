#include "visionrt/contracts/TrainingSet.h"

#include "visionrt/contracts/Value.h"

#include <string>

namespace visionrt {

TrainingImage trainingFrame(TrainingImageId id, const cv::Mat &frame, ColorOrder colorOrder)
{
    TrainingImage image;
    image.id         = id;
    image.pixels     = frame.clone();
    image.colorOrder = colorOrder;
    return image;
}

void validateTrainingSet(const TrainingSet &set)
{
    for (const auto &[id, image] : set.images)
    {
        if (image.pixels.empty())
            continue;
        try
        {
            validateImageFrame(image.pixels);
        }
        catch (const ContractError &error)
        {
            throw ContractError("训练图像 " + std::to_string(id) + "：" + error.what());
        }
    }
    for (const auto &[id, record] : set.records)
    {
        if (record.imageId && !set.images.contains(*record.imageId))
            throw ContractError("训练记录 " + std::to_string(id) + " 引用了不存在的图像 "
                                + std::to_string(*record.imageId));
        for (const auto &[field, value] : record.fields)
            if (const auto type = typeOf(value)) validateValue(*type, value);
    }
}

void validateTrainingSetPixels(const TrainingSet &set)
{
    validateTrainingSet(set);
    for (const auto &[id, image] : set.images)
    {
        try
        {
            validateImageFrame(image.pixels);
        }
        catch (const ContractError &error)
        {
            throw ContractError("训练图像 " + std::to_string(id) + "：" + error.what());
        }
    }
}

} // namespace visionrt
