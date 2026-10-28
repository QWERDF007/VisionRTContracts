#include <visionrt/contracts/Parameters.h>

#include <cassert>
#include <iostream>
#include <optional>
#include <string>

using namespace visionrt;

namespace {
struct SampleInputs final : InputParametersBase
{
    std::optional<DataRef<Image>> images;
    double                        gain   = -1.0;
    double                        offset = -1.0;
    std::int64_t                  count  = 0;
    std::optional<DataRef<ROI>>   mask;
    std::optional<DataRef<std::vector<bool>>> flags;

    ToolStatus readFields(InputReader &reader) override
    {
        auto images = reader.nullableImage("images");
        if (!images)
            return {1001, "图像输入不合法", images.issue().describe()};
        auto gain = reader.real("gain");
        if (!gain)
            return {1002, "增益不合法", gain.issue().describe()};
        auto offset = reader.real("offset");
        if (!offset)
            return {1002, "偏移不合法", offset.issue().describe()};
        auto count = reader.optionalInteger("count");
        if (!count)
            return {1002, "计数不合法", count.issue().describe()};
        auto mask = reader.nullableRoi("mask");
        if (!mask)
            return {1001, "掩膜不合法", mask.issue().describe()};
        auto flags = reader.nullableArray<std::vector<bool>>("flags");
        if (!flags)
            return {1003, "标志形状不合法", flags.issue().describe()};

        this->images = images.value();
        this->gain   = gain.value();
        this->offset = offset.value();
        this->count  = count.value().value_or(0);
        this->mask   = mask.value();
        this->flags  = flags.value();
        return {};
    }
};

struct SampleOutputs final : OutputParametersBase
{
    std::optional<DataRef<Image>> images;
    std::optional<DataRef<ROI>>   appliedMask;

    void writeFields(OutputWriter &writer) override
    {
        writer.optionalImage("images", images);
        writer.optionalRoi("applied_mask", appliedMask);
    }
};

struct BadOutputs final : OutputParametersBase
{
    enum class Mode
    {
        Reserved,
        Duplicate
    } mode = Mode::Reserved;

    void writeFields(OutputWriter &writer) override
    {
        if (mode == Mode::Reserved)
            writer.integer("__status_code", 0);
        else
        {
            writer.integer("value", 1);
            writer.integer("value", 2);
        }
    }
};
} // namespace

int main()
try
{
    const auto          image = DataRef<Image>(Image{cv::Mat(2, 3, CV_8UC1, cv::Scalar(9))});
    std::weak_ptr<void> weak  = image.owner();
    const auto          mask  = DataRef<ROI>(ROI{{{2, 0, 0, 2, 2, 0}}});
    SampleInputs        inputs;
    const auto          accepted = inputs.parse({{"upstream_status", std::int64_t{7}},
                                                 {"images", image},
                                                 {"gain", 2.5},
                                                 {"offset", -1.0},
                                                 {"count", std::int64_t{4}},
                                                 {"mask", mask},
                                                 {"flags", Array(DataRef<std::vector<bool>>(std::vector<bool>{true, false}))}});
    // A non-zero upstream status never short-circuits parsing: each tool decides what to do with it.
    assert(accepted.code == 0);
    assert(inputs.upstreamStatus == 7);
    assert(inputs.images && inputs.images->get().size() == 1);
    assert(inputs.gain == 2.5 && inputs.offset == -1.0 && inputs.count == 4);
    assert(inputs.mask && inputs.flags && inputs.flags->get().size() == 2);
    assert(!weak.expired());

    // A missing key and an explicit Missing value both mean "no value"; the fields stay unset.
    SampleInputs absent;
    assert(absent.parse({{"gain", 1.0}, {"offset", 0.0}, {"images", Missing{}}}).code == 0);
    assert(absent.upstreamStatus == 0);
    assert(!absent.images && !absent.mask && !absent.flags);

    // A wrong type is a reported issue that names the port, never a silent default and never a throw.
    SampleInputs wrong;
    const auto   typeIssue = wrong.parse({{"gain", std::int64_t{2}}, {"offset", 0.0}});
    assert(typeIssue.code == 1002 && typeIssue.detail.find("gain") != std::string::npos);
    assert(wrong.gain == -1.0);
    const auto boolIsNotInt = wrong.parse({{"gain", 1.0}, {"offset", 0.0}, {"count", true}});
    assert(boolIsNotInt.code == 1002 && boolIsNotInt.detail.find("count") != std::string::npos);
    const auto absentField = wrong.parse({{"offset", 0.0}});
    assert(absentField.code == 1002 && absentField.detail.find("gain") != std::string::npos);
    const auto rankMismatch = wrong.parse({{"gain", 1.0},
                                           {"offset", 0.0},
                                           {"flags", Array(NestedVector<bool, 2>{{true}})}});
    assert(rankMismatch.code == 1003 && rankMismatch.detail.find("flags") != std::string::npos);

    // The reader never throws: an absent port is an issue the caller decides about.
    const Values      readerValues{{"gain", std::int64_t{2}}};
    const InputReader reader(readerValues);
    assert(reader.integer("gain").ok() && reader.integer("gain").value() == 2);
    assert(!reader.real("gain"));
    assert(reader.real("gain").issue().kind == PortIssueKind::TypeMismatch);
    assert(!reader.real("absent"));
    assert(reader.real("absent").issue().kind == PortIssueKind::Missing);
    assert(!reader.present("absent") && reader.find("absent") == nullptr);
    assert(reader.optionalReal("absent").ok() && !reader.optionalReal("absent").value());

    // Outputs are written once each; a missing declared port becomes an explicit Missing.
    SampleOutputs outputs;
    outputs.images = image;
    outputs.status = {7, "业务失败"};
    const auto packed = outputs.pack();
    assert(packed.status.code == 7 && packed.status.message == "业务失败");
    assert(std::get<DataRef<Image>>(packed.outputs.at("images")).get().size() == 1);
    assert(isMissing(packed.outputs.at("applied_mask")));
    assert(!packed.outputs.contains(std::string(kStatusCodePort)));
    assert(weak.lock() != nullptr);

    BadOutputs reserved;
    const auto rejected = reserved.pack();
    assert(rejected.status.code == kContractViolationStatus);
    assert(rejected.status.detail.find("__status_code") != std::string::npos);
    BadOutputs duplicate;
    duplicate.mode = BadOutputs::Mode::Duplicate;
    assert(duplicate.pack().status.detail.find("重复") != std::string::npos);

    // The writer rejects a duplicate immediately and keeps the first value.
    OutputWriter writer;
    assert(!writer.integer("value", 1));
    assert(writer.integer("value", 2).has_value());
    assert(writer.firstIssue()->kind == PortIssueKind::DuplicateOutput);
    assert(std::get<std::int64_t>(writer.take().at("value")) == 1);

    std::cout << "Typed parameters report issues without throwing and keep status policy with the tool\n";
    return 0;
}
catch (const std::exception &error)
{
    std::cerr << error.what() << '\n';
    return 1;
}
