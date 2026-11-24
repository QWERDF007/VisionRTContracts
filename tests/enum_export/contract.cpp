#include "EnumExport.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

#define VRT_ENUM_FILE "FixtureEnums.def"
#define VRT_ENUM_HEADER "Fixture.h"
#define VRT_ENUM_CATALOG fixture
#include "EnumCatalog.h"

int main(int argc, char **argv)
try
{
    if (argc != 2)
        throw std::runtime_error("expected scratch directory");
    const auto root = std::filesystem::path(argv[1]);
    const auto output = root / "enums.py";
    const auto path = output.string();
    const char *pythonArgs[]{"export", "--python", path.c_str()};
    const visionrt::codegen::WireName wires[]{{"State", "Ready", "ready"}};
    if (visionrt::codegen::run(3, pythonArgs, {fixture::enums}, wires))
        throw std::runtime_error("Python generation failed");
    auto read = [](const auto &file) {
        std::ifstream input(file, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(input), {});
    };
    const auto python = read(output);
    for (const auto expected : {"class State(IntEnum):", "None_ = 0", "Ready = 7",
             "class CommonError(IntEnum):", "INPUT_MISSING = 1001", "COMMON_MESSAGES = {",
             "1001: \"输入\\\"缺失\\\"\\n请检查\"", "\"ready\": State.Ready"})
        if (python.find(expected) == std::string::npos)
            throw std::runtime_error(std::string("missing Python export: ") + expected);
    const auto timestamp = std::filesystem::last_write_time(output);
    if (visionrt::codegen::run(3, pythonArgs, {fixture::enums}, wires)
        || timestamp != std::filesystem::last_write_time(output))
        throw std::runtime_error("identical export must preserve output timestamp");
    const auto qmlOutput = root / "UiEnums.h";
    const auto qmlPath = qmlOutput.string();
    const char *qmlArgs[]{"export", "--qml", qmlPath.c_str()};
    if (visionrt::codegen::run(3, qmlArgs, {fixture::enums}))
        throw std::runtime_error("QML generation failed");
    const auto qml = read(qmlOutput);
    for (const auto expected : {"#include \"Fixture.h\"", "Q_GADGET", "Q_ENUM(Value)",
             "Ready = static_cast<int>(visionrt::State::Ready)"})
        if (qml.find(expected) == std::string::npos)
            throw std::runtime_error(std::string("missing QML export: ") + expected);
    const char *badArgs[]{"export", "--unknown", path.c_str()};
    if (!visionrt::codegen::run(3, badArgs, {fixture::enums}) || read(output) != python)
        throw std::runtime_error("invalid CLI must fail without changing output");
    std::cout << "Native enum export contract passed\n";
    return 0;
}
catch (const std::exception &error)
{
    std::cerr << error.what() << '\n';
    return 1;
}
