#ifdef NDEBUG
#undef NDEBUG
#endif

#include "visionrt/sdk/Package.h"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace visionrt;

namespace {

const char* kToolAlpha = R"(schema_version: 1
tool_id: test.pkg.alpha
display_name: 包内工具甲
version: 1.2.0
kind: cpp_function
entry:
  registry_name: test.pkg.alpha
init_params: []
inputs:
  - id: x
    name: 输入
    type: Int
outputs:
  - id: y
    name: 输出
    type: Int
)";

const char* kToolBravo = R"(schema_version: 1
tool_id: test.pkg.bravo
display_name: 包内工具乙
version: 1.0.0
kind: cpp_class
entry:
  registry_name: test.pkg.bravo.registry
init_params: []
inputs: []
outputs:
  - id: count
    name: 计数
    type: Int
)";

const char* kToolGamma = R"(schema_version: 1
tool_id: test.pkg.gamma
display_name: 自定义工具
version: 1.0.0
kind: cpp_function
entry:
  registry_name: test.pkg.gamma.registry
init_params: []
inputs: []
outputs:
  - id: value
    name: 值
    type: Float
)";

const char* kValidIndex = R"(variant: cpu
modules:
  mod_a:
    library: bin/mod_a.dll
tools:
  test.pkg.bravo.registry:
    module: mod_a
    definition: assets/tools/builtin/类别/组二/bravo.yaml
  test.pkg.alpha:
    module: mod_a
    definition: assets/tools/builtin/类别/组一/alpha.yaml
  test.pkg.gamma.registry:
    module: mod_a
    definition: assets/tools/custom/Python/示例/gamma.yaml
runtime_files:
  - bin/runtime_dep.dat
)";

const auto kAlphaPath = std::filesystem::path(u8"assets/tools/builtin/类别/组一/alpha.yaml");
const auto kBravoPath = std::filesystem::path(u8"assets/tools/builtin/类别/组二/bravo.yaml");
const auto kGammaPath = std::filesystem::path(u8"assets/tools/custom/Python/示例/gamma.yaml");

void writeFile(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    assert(out);
    out << content;
    assert(out.good());
}

std::filesystem::path makeRoot(const std::filesystem::path& base, const std::filesystem::path& name) {
    const auto root = base / name;
    std::error_code error;
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(root / "bin");
    std::filesystem::create_directories(root / "assets" / "tools");
    writeFile(root / "bin" / "mod_a.dll", "dummy module");
    writeFile(root / "bin" / "runtime_dep.dat", "dummy runtime");
    writeFile(root / kAlphaPath, kToolAlpha);
    writeFile(root / kBravoPath, kToolBravo);
    writeFile(root / kGammaPath, kToolGamma);
    writeFile(root / "module_index.yaml", kValidIndex);
    return root;
}

template <typename Callable>
bool failsWith(Callable&& callable, const std::string& fragment) {
    try {
        callable();
    } catch (const std::exception& error) {
        const std::string message = error.what();
        if (message.find(fragment) != std::string::npos) return true;
        std::cerr << "unexpected diagnostic: " << message << "\n(expected fragment: " << fragment << ")\n";
        return false;
    }
    std::cerr << "expected failure containing: " << fragment << "\n";
    return false;
}

void readValidPackage(const std::filesystem::path& root) {
    const auto package = readToolsPackage(root);
    assert(package.root == std::filesystem::absolute(root).lexically_normal());
    assert(package.index.variant == "cpu");
    assert(package.index.tools.size() == 3);
    assert(package.index.modules.size() == 1);
    assert(package.tools.size() == 3);

    // Document order of the index is not trusted; tools are deterministic by declaration path.
    assert(package.tools[0].toolId == "test.pkg.alpha");
    assert(package.tools[1].toolId == "test.pkg.bravo");
    assert(package.tools[2].toolId == "test.pkg.gamma");
    assert(package.tools[0].registryName == "test.pkg.alpha");
    assert(package.tools[1].registryName == "test.pkg.bravo.registry");
    assert(package.tools[2].registryName == "test.pkg.gamma.registry");
    assert(package.tools[0].definition.displayName == "包内工具甲");
    assert(package.tools[0].definition.version == "1.2.0");
    assert(package.tools[0].moduleLibrary == package.root / "bin" / "mod_a.dll");
    assert(std::filesystem::is_regular_file(package.tools[0].moduleLibrary));
    assert(package.tools[0].definition.definitionPath ==
           package.root / std::filesystem::path(u8"assets/tools/builtin/类别/组一/alpha.yaml"));
    assert(package.tools[1].definition.kind == ToolKind::CppClass);

    // Declarations under assets/tools/builtin ship as platform tools, everything else as custom tooling.
    assert(package.tools[0].definition.builtin);
    assert(package.tools[1].definition.builtin);
    assert(!package.tools[2].definition.builtin);
    assert(package.tools[2].definition.definitionPath == package.root / kGammaPath);

    // Validation entry delegates to the same reading semantics.
    validateToolsPackage(root);
}

void readInvalidPackages(const std::filesystem::path& base) {
    const auto root = makeRoot(base, "negatives");

    assert(failsWith([&] { validateToolsPackage(root / "missing_root"); },
                     "not a valid directory"));

    auto noIndex = makeRoot(base, "no_index");
    std::filesystem::remove(noIndex / "module_index.yaml");
    assert(failsWith([&] { readToolsPackage(noIndex); }, "Missing module_index.yaml in package"));

    auto brokenIndex = makeRoot(base, "broken_index");
    writeFile(brokenIndex / "module_index.yaml", "modules: [ unclosed\n");
    assert(failsWith([&] { readToolsPackage(brokenIndex); }, "YAML"));

    auto dangling = makeRoot(base, "dangling");
    writeFile(dangling / "module_index.yaml",
        "variant: cpu\nmodules:\n  mod_a:\n    library: bin/mod_a.dll\ntools:\n"
        "  test.pkg.alpha:\n    module: mod_missing\n    definition: assets/tools/builtin/类别/组一/alpha.yaml\n");
    assert(failsWith([&] { readToolsPackage(dangling); }, "references undeclared module"));

    auto unreferenced = makeRoot(base, "unreferenced");
    writeFile(unreferenced / "module_index.yaml",
        "variant: cpu\nmodules:\n  mod_a:\n    library: bin/mod_a.dll\n  mod_b:\n    library: bin/mod_b.dll\n"
        "tools:\n  test.pkg.alpha:\n    module: mod_a\n"
        "    definition: assets/tools/builtin/类别/组一/alpha.yaml\n");
    assert(failsWith([&] { readToolsPackage(unreferenced); }, "not referenced by any tool"));

    auto unknownField = makeRoot(base, "unknown_field");
    writeFile(unknownField / "module_index.yaml",
        "variant: cpu\nunknown_key: bad\nmodules:\n  mod_a:\n    library: bin/mod_a.dll\ntools:\n"
        "  test.pkg.alpha:\n    module: mod_a\n    definition: assets/tools/builtin/类别/组一/alpha.yaml\n");
    assert(failsWith([&] { readToolsPackage(unknownField); }, "unknown field"));

    auto missingLibrary = makeRoot(base, "missing_library");
    std::filesystem::remove(missingLibrary / "bin" / "mod_a.dll");
    assert(failsWith([&] { readToolsPackage(missingLibrary); }, "library file does not exist"));

    auto missingDefinition = makeRoot(base, "missing_definition");
    std::filesystem::remove(missingDefinition / kAlphaPath);
    assert(failsWith([&] { readToolsPackage(missingDefinition); }, "definition file does not exist"));

    auto registryMismatch = makeRoot(base, "registry_mismatch");
    writeFile(registryMismatch / kAlphaPath,
        R"(schema_version: 1
tool_id: test.pkg.alpha
display_name: 包内工具甲
version: 1.2.0
kind: cpp_function
entry:
  registry_name: test.pkg.other
init_params: []
inputs: []
outputs: []
)");
    assert(failsWith([&] { readToolsPackage(registryMismatch); }, "does not match index tool key"));

    auto missingRuntime = makeRoot(base, "missing_runtime");
    std::filesystem::remove(missingRuntime / "bin" / "runtime_dep.dat");
    assert(failsWith([&] { readToolsPackage(missingRuntime); }, "runtime_files entry does not exist"));

    auto duplicateIdentity = makeRoot(base, "duplicate_identity");
    writeFile(duplicateIdentity / kBravoPath,
        R"(schema_version: 1
tool_id: test.pkg.alpha
display_name: 重复身份工具
version: 1.0.0
kind: cpp_class
entry:
  registry_name: test.pkg.bravo.registry
init_params: []
inputs: []
outputs: []
)");
    assert(failsWith([&] { readToolsPackage(duplicateIdentity); }, "Duplicate tool_id"));

    auto duplicateKey = makeRoot(base, "duplicate_key");
    writeFile(duplicateKey / "module_index.yaml",
        "variant: cpu\nmodules:\n  mod_a:\n    library: bin/mod_a.dll\ntools:\n"
        "  test.pkg.alpha:\n    module: mod_a\n    definition: assets/tools/builtin/类别/组一/alpha.yaml\n"
        "  test.pkg.alpha:\n    module: mod_a\n    definition: assets/tools/builtin/类别/组二/bravo.yaml\n");
    assert(failsWith([&] { readToolsPackage(duplicateKey); }, "Duplicate key"));
}

} // namespace

int main() {
    const auto unique = std::to_string(static_cast<long long>(
        std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto base = std::filesystem::temp_directory_path() / ("vrt_sdk_package_" + unique);
    std::error_code error;
    std::filesystem::remove_all(base, error);
    std::filesystem::create_directories(base);

    try {
        // Chinese characters and spaces in the package path are ordinary paths.
        const auto unicodeRoot = makeRoot(base, std::filesystem::path(u8"测试 目录/工具 包 1.0"));
        readValidPackage(unicodeRoot);
        readInvalidPackages(base);
        std::filesystem::remove_all(base, error);
        std::cout << "sdk package contract passed\n";
        return 0;
    } catch (const std::exception& failure) {
        std::cerr << "SDK package contract failed: " << failure.what() << '\n';
        std::filesystem::remove_all(base, error);
        return 1;
    }
}
