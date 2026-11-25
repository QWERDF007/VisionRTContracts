#include "visionrt/contracts/Module.h"
#include "visionrt/contracts/TrainingSet.h"
#include "visionrt/contracts/Value.h"
#include "visionrt/contracts/Parameters.h"

#include <cstdlib>
#include <iostream>
#include <string>

using namespace visionrt;

class ParameterTrainer final : public IToolInstance
{
public:
    ToolStatus initialize(const ToolConfiguration &) override { return {}; }
    ToolResult execute(const Values &) override { return toolResult({}); }
    ToolResult train(const Values &parameters) override
    {
        InputReader reader(parameters);
        const auto directions = reader.direction(0);
        const auto data = reader.trainingSet(1);
        if (!directions || !data) throw ContractError("typed training input was lost");
        return toolResult({{2, directions.value()}, {3, static_cast<std::int64_t>(data.value().get().records.size())}});
    }
    ToolStatus release() override { return {}; }
};

namespace {

void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

TrainingSet twoRecordsOneImage()
{
    const cv::Mat frame(4, 6, CV_8UC3, cv::Scalar(10, 20, 30));
    TrainingSet   set;
    set.images.emplace(7, trainingFrame(7, frame, ColorOrder::BGR));

    TrainingRecord first;
    first.id      = 1;
    first.imageId = 7;
    first.fields.emplace(1, std::string("blue"));
    first.fields.emplace(2, Rectangle::fromEncoded(ROI{{{2, 1.0, 1.0, 2.0, 2.0, 0.0}}}));
    set.records.emplace(first.id, first);

    TrainingRecord second;
    second.id      = 2;
    second.imageId = 7;
    second.fields.emplace(1, std::string("red"));
    second.fields.emplace(3, std::int64_t{9007199254740993LL});
    set.records.emplace(second.id, second);
    return set;
}

} // namespace

int main()
try
{
    static_assert(MODULE_ABI_MAJOR == 16);

    TrainingSet numeric;
    TrainingRecord record;
    record.id = 8;
    record.fields.emplace(1, Direction::fromNested({{{10, 20, 5, 90}}}));
    record.fields.emplace(2, DataRef<Image>(Image{cv::Mat(2, 2, CV_8UC1, cv::Scalar(17))}));
    numeric.records.emplace(record.id, record);
    validateTrainingSet(numeric);
    ParameterTrainer trainer;
    const auto trained = trainer.train({{0, Direction::fromNested({{{10, 20, 5, 90}}})}, {1, DataRef<TrainingSet>(numeric)}});
    require(std::get<Direction>(trained.outputs.at(2)).at(0, 0).end() == cv::Point2d(10, 25), "training reads declared direction parameter");
    require(std::get<std::int64_t>(trained.outputs.at(3)) == 1, "training set is an ordinary numbered parameter");

    auto set = twoRecordsOneImage();
    validateTrainingSet(set);
    require(set.images.size() == 1 && set.records.size() == 2, "one image, two records");
    require(set.records.at(1).imageId == set.records.at(2).imageId, "records name the same frame");

    const PortValue value = DataRef<TrainingSet>(set);
    require(typeOf(value) == TypeSpec{Type::TrainingSet}, "training set has its own type");
    validateValue({Type::TrainingSet}, value);

    // The frame a training image owns is a copy: changing the batch afterwards cannot change it.
    cv::Mat mutableFrame(3, 5, CV_8UC1, cv::Scalar(1));
    auto    image = trainingFrame(9, mutableFrame, ColorOrder::Gray);
    mutableFrame.setTo(cv::Scalar(200));
    require(image.pixels.at<unsigned char>(0, 0) == 1, "training image keeps its own pixels");

    // A retained set survives the value it was copied from.
    std::optional<DataRef<TrainingSet>> retained;
    {
        const PortValue local = DataRef<TrainingSet>(set);
        retained              = std::get<DataRef<TrainingSet>>(retainValue(local, std::make_shared<int>(1)));
    }
    require(retained->get().images.at(7).pixels.at<unsigned char>(0, 0) == 10, "retained pixels readable");

    // A record that names an image outside the table never reaches a trainer.
    auto broken = set;
    broken.records.at(2).imageId = 99;
    bool rejected = false;
    try
    {
        validateTrainingSet(broken);
    }
    catch (const ContractError &error)
    {
        rejected = std::string(error.what()).find("不存在的图像") != std::string::npos;
    }
    require(rejected, "missing image is rejected by number");

    // Persisted metadata-only frames pass validateTrainingSet, while validateTrainingSetPixels rejects empty frames.
    TrainingSet emptyFrame;
    emptyFrame.images.emplace(1, trainingFrame(1, cv::Mat{}, ColorOrder::BGR));
    validateTrainingSet(emptyFrame);
    rejected = false;
    try
    {
        validateTrainingSetPixels(emptyFrame);
    }
    catch (const ContractError &)
    {
        rejected = true;
    }
    require(rejected, "empty frame is rejected by validateTrainingSetPixels");

    require(std::get<DataRef<TrainingSet>>(defaultValue(TypeSpec{Type::TrainingSet})).get().records.empty(), "training set default is an empty typed value");

    std::cout << "training set keeps exact keys, shared images and its own pixels\n";
    return 0;
}
catch (const std::exception &error)
{
    std::cerr << error.what() << '\n';
    return 1;
}
