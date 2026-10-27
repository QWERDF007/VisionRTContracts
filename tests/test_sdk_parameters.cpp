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
    double                        gain   = 1.0;
    double                        offset = 0.0;
    std::int64_t                  count  = 0;
    std::optional<DataRef<ROI>>   mask;
    std::optional<DataRef<std::vector<bool>>> flags;

    void readFields(InputReader &reader) override
    {
        images = reader.nullableImage("images");
        gain   = reader.real("gain");
        offset = reader.real("offset");
        count  = reader.optionalInteger("count").value_or(0);
        mask   = reader.nullableRoi("mask");
        flags  = reader.nullableArray<std::vector<bool>>("flags");
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
    int mode = 0;

    void writeFields(OutputWriter &writer) override
    {
        if (mode == 1)
            writer.integer("__status_code", 0);
        else
        {
            writer.integer("value", 1);
            writer.integer("value", 2);
        }
    }
};

template<class Operation>
std::string failure(Operation &&operation)
{
    try
    {
        operation();
    }
    catch (const ContractError &error)
    {
        return error.what();
    }
    return {};
}
} // namespace

int main()
try
{
    const auto           image = DataRef<Image>(Image{cv::Mat(2, 3, CV_8UC1, cv::Scalar(9))});
    std::weak_ptr<void>  weak  = image.owner();
    const auto           mask  = DataRef<ROI>(ROI{{{2, 0, 0, 2, 2, 0}}});
    SampleInputs         inputs;
    inputs.parse({{"upstream_status", std::int64_t{7}},
                  {"images", image},
                  {"gain", 2.5},
                  {"offset", -1.0},
                  {"count", std::int64_t{4}},
                  {"mask", mask},
                  {"flags", Array(DataRef<std::vector<bool>>(std::vector<bool>{true, false}))}});
    // A non-zero upstream status never short-circuits parsing: each tool decides what to do with it.
    assert(inputs.upstreamStatus == 7);
    assert(inputs.images && inputs.images->get().size() == 1);
    assert(inputs.gain == 2.5 && inputs.offset == -1.0 && inputs.count == 4);
    assert(inputs.mask && inputs.flags && inputs.flags->get().size() == 2);
    assert(!weak.expired());

    // A missing key and an explicit Missing value both mean "no value"; the fields stay unset.
    SampleInputs absent;
    absent.parse({{"gain", 1.0}, {"offset", 0.0}, {"images", Missing{}}});
    assert(absent.upstreamStatus == 0);
    assert(!absent.images && !absent.mask && !absent.flags);

    // Wrong types are errors, not silent defaults.
    SampleInputs wrong;
    const auto   typeError = failure([&] { wrong.parse({{"gain", std::int64_t{2}}, {"offset", 0.0}}); });
    assert(typeError.find("gain") != std::string::npos);
    const auto boolIsNotInt = failure([&] { wrong.parse({{"gain", 1.0}, {"offset", 0.0}, {"count", true}}); });
    assert(boolIsNotInt.find("count") != std::string::npos);
    const auto absentField = failure([&] { wrong.parse({{"offset", 0.0}}); });
    assert(absentField.find("gain") != std::string::npos);
    const auto rankMismatch = failure(
        [&]
        {
            wrong.parse({{"gain", 1.0},
                         {"offset", 0.0},
                         {"flags", Array(NestedVector<bool, 2>{{true}})}});
        });
    assert(rankMismatch.find("flags") != std::string::npos);

    // Outputs are written once each; a missing declared port becomes an explicit Missing.
    SampleOutputs outputs;
    outputs.images = image;
    outputs.status = {7, ""};
    const auto packed = outputs.pack();
    assert(std::get<std::int64_t>(packed.outputs.at(std::string(kStatusCodePort))) == 7);
    assert(std::get<DataRef<Image>>(packed.outputs.at("images")).get().size() == 1);
    assert(isMissing(packed.outputs.at("applied_mask")));
    assert(weak.lock() != nullptr);

    BadOutputs reserved;
    reserved.mode = 1;
    assert(!failure([&] { reserved.pack(); }).empty());
    BadOutputs duplicate;
    duplicate.mode = 2;
    assert(failure([&] { duplicate.pack(); }).find("重复") != std::string::npos);

    // A successful tool still decides its own message; the base class adds nothing.
    SampleOutputs success;
    success.status = {0, ""};
    const auto blank = success.pack();
    assert(std::get<std::string>(blank.outputs.at(std::string(kStatusMessagePort))).empty());

    std::cout << "Typed parameters keep Values ownership and status policy with the tool\n";
    return 0;
}
catch (const std::exception &error)
{
    std::cerr << error.what() << '\n';
    return 1;
}
