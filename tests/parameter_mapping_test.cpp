#include <visionrt/contracts/Parameters.h>

#include <cassert>
#include <iostream>
#include <optional>
#include <string>

using namespace visionrt;

namespace {
// The numbers below are this fixture tool's published ports; the reader never sees a spelling.
namespace in {
constexpr PortId Images = 1, Gain = 2, Offset = 3, Count = 4, Mask = 5, Flags = 6;
}
namespace out {
constexpr PortId Images = 2, AppliedMask = 3;
}

struct SampleInputs final : InputParametersBase
{
    std::optional<DataRef<Image>> images;
    double                        gain   = -1.0;
    double                        offset = -1.0;
    std::int64_t                  count  = 0;
    std::optional<DataRef<ROI>>   mask;
    std::optional<Array>            flags;

    ToolStatus readFields(InputReader &reader) override
    {
        auto images = reader.nullableImage(in::Images);
        if (!images)
            return {1001, "图像输入不合法", images.issue().describe()};
        auto gain = reader.real(in::Gain);
        if (!gain)
            return {1002, "增益不合法", gain.issue().describe()};
        auto offset = reader.real(in::Offset);
        if (!offset)
            return {1002, "偏移不合法", offset.issue().describe()};
        auto count = reader.optionalInteger(in::Count);
        if (!count)
            return {1002, "计数不合法", count.issue().describe()};
        auto mask = reader.nullableRoi(in::Mask);
        if (!mask)
            return {1001, "掩膜不合法", mask.issue().describe()};
        auto flags = reader.nullableArray(in::Flags);
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
        writer.optionalImage(out::Images, images);
        writer.optionalRoi(out::AppliedMask, appliedMask);
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
            writer.integer(kStatusCodePort, 0);
        else
        {
            writer.integer(2, 1);
            writer.integer(2, 2);
        }
    }
};
} // namespace

int main()
try
{
    // The parsed parameter, not the caller, must keep the image alive: the source leaves scope
    // before the lifetime is checked, so an expired weak pointer would prove a missing reference.
    std::weak_ptr<void> weak;
    const auto          mask = DataRef<ROI>(ROI{{{2, 0, 0, 2, 2, 0}}});
    SampleInputs        inputs;
    ToolStatus          accepted;
    {
        const auto image = DataRef<Image>(Image{cv::Mat(2, 3, CV_8UC1, cv::Scalar(9))});
        weak             = image.owner();
        accepted         = inputs.parse({{kUpstreamStatusPort, std::int64_t{7}},
                                 {in::Images, image},
                                 {in::Gain, 2.5},
                                 {in::Offset, -1.0},
                                 {in::Count, std::int64_t{4}},
                                 {in::Mask, mask},
                                 {in::Flags, Array::fromNested(std::vector<bool>{true, false})}});
    }
    // A non-zero upstream status never short-circuits parsing: each tool decides what to do with it.
    assert(accepted.code == 0);
    assert(inputs.upstreamStatus == 7);
    assert(inputs.images && inputs.images->get().size() == 1);
    assert(inputs.gain == 2.5 && inputs.offset == -1.0 && inputs.count == 4);
    assert(inputs.mask && inputs.flags && inputs.flags->elementCount() == 2);
    assert(!weak.expired());

    // A missing key and an explicit Missing value both mean "no value"; the fields stay unset.
    SampleInputs absent;
    assert(absent.parse({{in::Gain, 1.0}, {in::Offset, 0.0}, {in::Images, Missing{}}}).code == 0);
    assert(absent.upstreamStatus == 0);
    assert(!absent.images && !absent.mask && !absent.flags);

    // A wrong type is a reported issue that names the port, never a silent default and never a throw.
    SampleInputs wrong;
    const auto   typeIssue = wrong.parse({{in::Gain, std::int64_t{2}}, {in::Offset, 0.0}});
    assert(typeIssue.code == 1002 && typeIssue.detail.find("端口 2") != std::string::npos);
    assert(wrong.gain == -1.0);
    const auto boolIsNotInt = wrong.parse({{in::Gain, 1.0}, {in::Offset, 0.0}, {in::Count, true}});
    assert(boolIsNotInt.code == 1002 && boolIsNotInt.detail.find("端口 4") != std::string::npos);
    const auto absentField = wrong.parse({{in::Offset, 0.0}});
    assert(absentField.code == 1002 && absentField.detail.find("端口 2") != std::string::npos);
    const auto rankMismatch = wrong.parse({{in::Gain, 1.0},
                                           {in::Offset, 0.0},
                                           {in::Flags, std::int64_t{1}}});
    assert(rankMismatch.code == 1003 && rankMismatch.detail.find("端口 6") != std::string::npos);

    // The reader never throws: an absent port is an issue the caller decides about.
    const Values      readerValues{{in::Gain, std::int64_t{2}}};
    const InputReader reader(readerValues);
    assert(reader.integer(in::Gain).ok() && reader.integer(in::Gain).value() == 2);
    assert(!reader.real(in::Gain));
    assert(reader.real(in::Gain).issue().kind == PortIssueKind::TypeMismatch);
    assert(!reader.real(99));
    assert(reader.real(99).issue().kind == PortIssueKind::Missing);
    assert(!reader.present(99) && reader.find(99) == nullptr);
    assert(reader.optionalReal(99).ok() && !reader.optionalReal(99).value());

    // Outputs are written once each; a missing declared port becomes an explicit Missing.
    SampleOutputs outputs;
    outputs.images = DataRef<Image>(Image{cv::Mat(2, 3, CV_8UC1, cv::Scalar(9))});
    outputs.status = {7, "业务失败"};
    const auto packed = outputs.pack();
    assert(std::get<std::int64_t>(packed.outputs.at(kStatusCodePort)) == 7); assert(std::get<std::string>(packed.outputs.at(kStatusMessagePort)) == "业务失败");
    assert(std::get<DataRef<Image>>(packed.outputs.at(out::Images)).get().size() == 1);
    assert(isMissing(packed.outputs.at(out::AppliedMask)));

    assert(weak.lock() != nullptr);

    for (std::int64_t code : {0, 7}) { BadOutputs reserved; reserved.status = {code, "test"}; bool caughtReserved = false; try { reserved.pack(); } catch (const ContractError &err) { caughtReserved = std::string(err.what()).find("端口 0") != std::string::npos; } assert(caughtReserved); BadOutputs duplicate; duplicate.mode = BadOutputs::Mode::Duplicate; duplicate.status = {code, "test"}; bool caughtDuplicate = false; try { duplicate.pack(); } catch (const ContractError &err) { caughtDuplicate = std::string(err.what()).find("重复") != std::string::npos; } assert(caughtDuplicate); }




    // The writer rejects a duplicate immediately and keeps the first value.
    OutputWriter writer;
    assert(!writer.integer(2, 1));
    assert(writer.integer(2, 2).has_value());
    assert(writer.firstIssue()->kind == PortIssueKind::DuplicateOutput);
    assert(std::get<std::int64_t>(writer.take().at(2)) == 1);

    std::cout << "Typed parameters report issues without throwing and keep status policy with the tool\n";
    return 0;
}
catch (const std::exception &error)
{
    std::cerr << error.what() << '\n';
    return 1;
}
