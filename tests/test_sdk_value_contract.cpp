#include <visionrt/contracts/Value.h>
#include <visionrt/contracts/Tool.h>
#include <visionrt/contracts/Module.h>

#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>

using namespace visionrt;

namespace {
template<class Operation>
void rejects(Operation &&operation)
{
    bool rejected = false;
    try { operation(); } catch (const std::exception&) { rejected = true; }
    assert(rejected);
}
}

int main()
try
{
    static_assert(MODULE_ABI_MAJOR == 6);
    static_assert(std::is_same_v<std::variant_alternative_t<4, PortValue>, std::string>);
    assert(std::string_view(typeName(Type::Image)) == "Image");
    assert(std::string_view(typeName(Type::Array)) == "Array");
    assert(std::string_view(typeName(Type::ROI)) == "ROI");
    assert(std::string_view(dtypeName(DType::String)) == "string");
    rejects([] { typeName(static_cast<Type>(-1)); });
    rejects([] { dtypeName(static_cast<DType>(999)); });
    rejects([] { validateTypeSpec({static_cast<Type>(999)}); });
    rejects([] { validateTypeSpec({Type::Array, static_cast<DType>(999), 1}); });
    rejects([] { validateTypeSpec({Type::Array}); });
    rejects([] { validateTypeSpec({Type::Array, DType::Int32, 0}); });
    rejects([] { validateTypeSpec({Type::Array, DType::Int32, 5}); });
    rejects([] { validateTypeSpec({Type::Image, DType::UInt8, 1}); });
    assert(!compatible({Type::Int}, {Type::Bool}));
    assert(!compatible({Type::Array, DType::Int64, 2}, {Type::Array, DType::Int64, 3}));
    assert(!compatible({Type::Array, DType::Int32, 2}, {Type::Array, DType::Int64, 2}));
    validateValue({Type::Image}, Missing{});
    assert(!typeOf(Missing{}));
    assert(!isMissing(DataRef<Image>(Image{})));
    validateValue({Type::Image}, DataRef<Image>(Image{}));
    rejects([] { validateValue({Type::Int}, true); });
    const PortValue text = std::string("中文 🌍");
    validateValue({Type::String}, text);
    assert(equalValues(text, std::string("中文 🌍")));
    rejects([] { validateValue({Type::String}, std::string("\xc0\x80", 2)); });
    for (const auto &encoded : {std::string("\xed\xa0\x80", 3), std::string("\xf4\x90\x80\x80", 4),
                                std::string("\xe4\xb8", 2), std::string("\x80", 1)})
        rejects([&] { validateUtf8(encoded); });

    const DataRef<ROI> roi(ROI{
        {{0, 1.125, 2.25}, {1, 1, 2, 3, 4}, {2, 10, 20, 8, 4, 90}, {3, 1, 2, 3}, {4, 0, 0, 4, 0, 0, 4}, {5, 1, 2}},
        {}
    });
    validateValue({Type::ROI}, roi);
    assert(roiKind(roi.get()[0][0]) == RoiKind::Point);
    const auto corners = rectangleCorners(roi.get()[0][2]);
    assert(std::abs(corners[0].x - 16) < 1e-12);
    assert(std::abs(corners[0].y - 18) < 1e-12);
    assert(std::abs(corners[2].x - 12) < 1e-12);
    assert(std::abs(corners[2].y - 26) < 1e-12);
    const double subpixel = 16777217.125;
    const auto   precise  = rectangleCorners(std::array{2.0, subpixel, 1.0, 2.0, 4.0, 0.0});
    assert(precise[0].x == subpixel && precise[2].x == subpixel + 2);
    assert(!equalValues(DataRef<ROI>(ROI{}), DataRef<ROI>(ROI{{}})));
    const auto roiAlias     = roi;
    roiAlias.get()[0][0][1] = 9.5;
    assert(roi.get()[0][0][1] == 9.5);
    roiAlias.get()[0][0][0] = 0.5;
    rejects([&] { validateValue({Type::ROI}, roi); });
    for (const std::vector<double> &bad : std::initializer_list<std::vector<double>>{
             {},
             {0, 1},
             {1, 1, 2, 3},
             {2, 1, 2, 0, 3, 0},
             {2, 1, 2, 3, -1, 0},
             {3, 1, 2, 0},
             {4, 0, 0, 1, 1},
             {5},
             {6, 1, 2},
             {-1, 1, 2},
             {0, std::numeric_limits<double>::infinity(), 2},
             {0, 1, std::numeric_limits<double>::quiet_NaN()}
    })
        rejects([&] { validateRoi(bad); });
    rejects([] { rectangleCorners(std::array{0.0, 1.0, 2.0}); });

    // ROI transforms: a circle keeps its true ellipse extent under a non-uniform alignment, and a
    // projective singularity is reported instead of being replaced by a coordinate that looks real.
    const std::vector<double>   circle{3.0, 0.0, 0.0, 5.0};
    const std::array<double, 9> identityMatrix{1, 0, 0, 0, 1, 0, 0, 0, 1};
    {
        const auto box = transformedRoiBounds(circle, identityMatrix);
        assert(box[0] == cv::Point2d(-5.0, -5.0) && box[1] == cv::Point2d(5.0, 5.0));
    }
    {
        // x scaled by two, y untouched: a 2:1 ellipse, not a circle of radius ten.
        const std::array<double, 9> stretched{2, 0, 0, 0, 1, 0, 0, 0, 1};
        const auto                  box = transformedRoiBounds(circle, stretched);
        assert(box[0] == cv::Point2d(-10.0, -5.0) && box[1] == cv::Point2d(10.0, 5.0));
    }
    {
        // Shear x' = x + 2y: the x half-extent grows to r*sqrt(5) while y stays r.
        const std::array<double, 9> shear{1, 2, 0, 0, 1, 0, 0, 0, 1};
        const auto                  box = transformedRoiBounds(circle, shear);
        assert(std::abs(box[1].x - 5.0 * std::sqrt(5.0)) < 1e-12);
        assert(std::abs(box[0].x + 5.0 * std::sqrt(5.0)) < 1e-12);
        assert(std::abs(box[1].y - 5.0) < 1e-12 && std::abs(box[0].y + 5.0) < 1e-12);
    }
    {
        // A quarter turn moves the centre without changing the extent.
        const std::array<double, 9> quarter{0, -1, 0, 1, 0, 0, 0, 0, 1};
        const auto                  box = transformedRoiBounds(circle, quarter);
        assert(std::abs(box[0].x + 5.0) < 1e-12 && std::abs(box[0].y + 5.0) < 1e-12);
        assert(std::abs(box[1].x - 5.0) < 1e-12 && std::abs(box[1].y - 5.0) < 1e-12);
    }
    {
        // The same transform written with a homogeneous scale of four must land in the same frame.
        const std::array<double, 9> homogeneous{4, 0, 12, 0, 4, 8, 0, 0, 4};
        const auto                  box = transformedRoiBounds(circle, homogeneous);
        assert(std::abs(box[0].x + 2.0) < 1e-12 && std::abs(box[0].y + 3.0) < 1e-12);
        assert(std::abs(box[1].x - 8.0) < 1e-12 && std::abs(box[1].y - 7.0) < 1e-12);
    }
    {
        // w = x, evaluated at the circle centre (0,0): the seed has no image in the target frame.
        // The message must name the singularity, so a caller can tell it apart from a shape error.
        bool diagnosed = false;
        try { transformedRoiBounds(circle, std::array<double, 9>{1, 0, 0, 0, 1, 0, 1, 0, 0}); }
        catch (const ContractError &error)
        {
            diagnosed = std::string_view(error.what()).find("singular") != std::string_view::npos;
        }
        assert(diagnosed);
    }
    // A non-finite matrix entry, and an affine matrix whose extents overflow, are both rejected
    // rather than returned as an infinite box.
    rejects([&] {
        transformedRoiBounds(circle, std::array<double, 9>{1, 0, 0, 0, 1, 0, 0, 0,
                                                           std::numeric_limits<double>::quiet_NaN()});
    });
    rejects([&] { transformedRoiBounds(circle, std::array<double, 9>{1e308, 0, 0, 0, 1, 0, 0, 0, 1e-308}); });

    const DataRef<Image> images(Image{cv::Mat(3, 7, CV_8UC1, cv::Scalar(17))});
    const auto           alias            = images;
    alias.get()[0].at<std::uint8_t>(2, 6) = 31;
    assert(images.get()[0].at<std::uint8_t>(2, 6) == 31);
    alias.get().push_back(images.get()[0](cv::Rect(1, 0, 4, 3)));
    validateValue({Type::Image}, images);
    assert(images.get()[1].step[0] == 7);
    assert(equalValues(images, alias));
    assert(!equalValues(images, DataRef<Image>(Image{images.get()[0].clone(), images.get()[1].clone()})));
    alias.get().push_back(cv::Mat{});
    rejects([&] { validateValue({Type::Image}, images); });
    const std::array<int, 3> dimensions{2, 3, 4};
    rejects([&] { validateValue({Type::Image}, DataRef<Image>(Image{cv::Mat(3, dimensions.data(), CV_8UC1)})); });

    const Array names(NestedVector<std::string, 2>{
        {},
        {"中文", "two"}
    });
    validateValue({Type::Array, DType::String, 2}, names);
    assert(equalValues(names, Array(NestedVector<std::string, 2>{
                                  {},
                                  {"中文", "two"}
    })));
    names.get<NestedVector<std::string, 2>>()[1][1] = std::string("\xc0\x80", 2);
    rejects([&] { validateValue({Type::Array, DType::String, 2}, names); });
    const Array flags(std::vector<bool>{true, false, true});
    validateValue({Type::Array, DType::Bool, 1}, flags);
    flags.get<std::vector<bool>>()[1] = true;
    assert(equalValues(flags, Array(std::vector<bool>{true, true, true})));
    assert(!equalValues(flags, Array(std::vector<std::int32_t>{1, 1, 1})));

    for (const PortValue &source : {PortValue(DataRef<Image>(Image{})), PortValue(DataRef<ROI>(ROI{{}}))})
    {
        std::weak_ptr<void> weak;
        PortValue           retained;
        {
            Owner dependency = std::make_shared<int>(7);
            weak             = dependency;
            retained         = retainValue(source, dependency);
            assert(equalValues(source, retained));
        }
        assert(!weak.expired());
        retained = Missing{};
        assert(weak.expired());
    }

    for (const std::string literal : {"9007199254740993", "9223372036854775807", "-9223372036854775808", "0", "-1"})
        assert(formatInt64(parseInt64(literal)) == literal);
    for (const auto *bad : {"", "+1", " 1", "1 ", "1.0", "1e2", "--1", "9223372036854775808", "-9223372036854775809"})
        rejects([&] { parseInt64(bad); });
    rejects([] { checkedElementCount(std::array<std::int64_t, 2>{-1, 0}); });
    rejects([] { checkedElementCount(std::array<std::int64_t, 2>{std::numeric_limits<std::int64_t>::max(), 3}); });
    rejects(
        []
        { checkedByteCount(std::array<std::int64_t, 1>{std::numeric_limits<std::int64_t>::max()}, sizeof(double)); });
    assert(checkedElementCount(std::array<std::int64_t, 3>{std::numeric_limits<std::int64_t>::max(), 3, 0}) == 0);

    for (const auto *blank : {"", " ", "\t\r\n", "\v\f"})
    {
        assert(isBlankMessage(blank));
        assert(executionSuccess(blank).message == "运行成功");
    }
    assert(!isBlankMessage("业务失败"));
    assert(executionSuccess("业务完成").message == "业务完成");

    // A tool result carries the business ports plus one status; the status is never folded into a
    // message and the two status ports stay host-owned until a node materialises them.
    const auto complete = toolResult({{"value", std::int64_t{9}}}, {17, "业务失败", "technical"});
    assert(complete.outputs.size() == 1);
    assert(std::get<std::int64_t>(complete.outputs.at("value")) == 9);
    assert(complete.status.code == 17);
    assert(complete.status.message == "业务失败");
    assert(complete.status.detail == "technical");
    assert(!complete.outputs.contains(std::string(kStatusCodePort)));
    assert(!complete.outputs.contains(std::string(kStatusMessagePort)));

    const auto success = toolResult({}, {});
    assert(success.status.message.empty());
    const auto negative = toolResult({}, {-5, "业务负数"});
    assert(negative.status.code == -5 && negative.status.message == "业务负数");
    assert(kCancelledStatus == -5);
    assert(isStatusPort(kStatusCodePort) && !isStatusPort("value"));

    // A path that needs a saved project is a reportable issue, not an exception.
    const PathResolver unsaved;
    const auto         unresolved = unsaved.tryResolve("images/a.png");
    assert(!unresolved && unresolved.issue().kind == PortIssueKind::PathUnresolved);
    assert(unresolved.issue().describe().find("a.png") != std::string::npos);
    const PathResolver saved(std::filesystem::path("C:/project"));
    const auto         relative = saved.tryResolve("images/a.png");
    assert(relative && relative.value() == std::filesystem::path("C:/project/images/a.png"));
    const auto absolute = unsaved.tryResolve("C:/images/a.png");
    assert(absolute && absolute.value() == std::filesystem::path("C:/images/a.png"));

    std::cout << "ABI6 writable value, ROI, result and scalar contracts verified\n";
    return 0;
}
catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
