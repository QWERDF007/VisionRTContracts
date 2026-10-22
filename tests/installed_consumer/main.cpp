#include "shared_probe.h"

#include <visionrt/contracts/Module.h>
#include <visionrt/contracts/SchemaYaml.h>
#include <visionrt/contracts/ValueYaml.h>

#include <algorithm>
#include <cassert>
#include <iostream>

#ifdef EXPECT_PYTHON_INTEROP
#include <visionrt/contracts/PythonInterop.h>
#include <pybind11/embed.h>
#include <pybind11/numpy.h>
#endif

using namespace visionrt;

int main()
{
    try
    {
        static_assert(MODULE_ABI_MAJOR == 2);
        assert(contractSharedProbe() == 8);
        const TypeSpec rectType{Type::Rect};
        const PortValue rectangle = Roi::rectangle(1.25F, -2.5F, 3.75F, 4.5F, 17.5F);
        assert(typeOf(rectangle) == rectType);
        assert(decodeTypeSpec(encodeTypeSpec(rectType)) == rectType);
        assert(equalValues(decodeValue(YAML::Load(YAML::Dump(encodeValue(rectangle, rectType))), rectType), rectangle));
        assert(isMissing(decodeValue(YAML::Load("null"), rectType)));
        const auto definition = parseToolDefinitionYaml(R"(
schema_version: 1
tool_id: installed.rect
display_name: Installed rectangle
version: 1.0.0
kind: cpp_function
entry: {registry_name: installed.rect}
init_params: []
inputs:
  - {id: images, name: Images, type: ImageBatch}
  - {id: rect, name: Rectangle, type: Rect, default: [2, 1.25, -2.5, 3.75, 4.5, 17.5], image_input: images}
outputs:
  - {id: images, name: Images, type: ImageBatch}
  - {id: rect, name: Rectangle, type: Rect, image_port: images}
)");
        assert(definition.inputs[1].type == rectType);
        assert(definition.inputs[1].imageInput == "images");
        const auto output = std::find_if(definition.outputs.begin(), definition.outputs.end(),
                                         [](const auto &port) { return port.id == "rect"; });
        assert(output != definition.outputs.end() && output->type == rectType && output->imagePort == "images");
        assert(equalValues(definition.inputs[1].defaultValue, rectangle));
        for (const auto *invalid : {"[3, 1, 2, 3]", "[2, 0, 0, 0, 5, 0]", "[[2, 0, 0, 4, 5, 0]]"})
        {
            bool rejected = false;
            try { (void)decodeValue(YAML::Load(invalid), rectType); }
            catch (const ContractError &) { rejected = true; }
            assert(rejected);
        }
        PortValue integer = std::int64_t{9007199254740993LL};
        assert(equalValues(decodeValue(encodeValue(integer, {Type::Int}), {Type::Int}), integer));
        std::cout << "[Standalone Consumer] Installed ABI 2, Rect Schema/YAML and integer contracts verified.\n";

#ifdef EXPECT_PYTHON_INTEROP
        namespace py = pybind11;
        PortValue imageResult;
        PortValue rectangleResult;
        {
            py::scoped_interpreter interpreter;
            auto scope = py::dict();
            {
                const auto image = OwnedImage::take(cv::Mat(3, 7, CV_8UC1, cv::Scalar(17))).region({1, 0, 4, 3});
                scope["images"] = toPythonReadonly(ImageBatch({image}));
                const auto array = py::cast<py::array>(py::cast<py::list>(scope["images"])[0]);
                assert(array.data() == image.data());
                assert(array.strides(0) == static_cast<py::ssize_t>(image.rowStride()));
            }
            py::exec("assert images[0].shape == (3, 4)\nassert not images[0].flags.writeable\nassert int(images[0][2, 3]) == 17", scope);
            imageResult = fromPython({Type::ImageBatch}, scope["images"]);
            std::weak_ptr<const std::vector<float>> lifetime;
            py::object view;
            {
                const auto source = Roi::rectangle(1.25F, -2.5F, 3.75F, 4.5F, 17.5F);
                lifetime = source.owner();
                view = toPythonReadonly(source);
                const auto array = py::cast<py::array>(view);
                assert(array.ndim() == 1 && array.shape(0) == 6);
                assert(array.dtype().equal(py::dtype::of<float>()) && !array.writeable());
                assert(array.data() == source.encoded().data());
            }
            assert(!lifetime.expired());
            rectangleResult = fromPython(rectType, view);
            assert(equalValues(rectangleResult, rectangle));
            assert(std::get<Roi>(rectangleResult).encoded().data() != py::cast<py::array>(view).data());
            view = py::none();
            assert(lifetime.expired());
            scope["labels"] = toPythonReadonly(StringArray(1, Layout::Dense, {StringBuffer({2}, {"cat", "dog"})}));
            py::exec("assert labels.tolist() == ['cat', 'dog']\nassert not labels.flags.writeable", scope);
            const auto numpy = py::module_::import("numpy");
            for (const auto *expression : {"[3, 1, 2, 3]", "[2, 0, 0, 0, 5, 0]", "[[2, 0, 0, 4, 5, 0]]"})
            {
                bool rejected = false;
                try { (void)fromPython(rectType, numpy.attr("array")(py::eval(expression), py::arg("dtype") = "float32")); }
                catch (const std::exception &) { rejected = true; }
                assert(rejected);
            }
            for (const auto *dtype : {"float64", "int32"})
            {
                bool rejected = false;
                try { (void)fromPython(rectType, numpy.attr("array")(py::eval("[2, 0, 0, 4, 5, 0]"), py::arg("dtype") = dtype)); }
                catch (const std::exception &) { rejected = true; }
                assert(rejected);
            }
            assert(isMissing(fromPython(rectType, py::none())));
        }
        const auto &image = std::get<ImageBatch>(imageResult).images()[0];
        assert(image.rows() == 3 && image.cols() == 4 && image.data()[0] == 17);
        assert(equalValues(rectangleResult, rectangle));
        std::cout << "[Standalone Consumer] Installed PythonInterop Rect/image owners survive interpreter shutdown.\n";
#endif
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
