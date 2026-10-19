#include "visionrt/sdk/Package.h"

#include <algorithm>
#include <set>
#include <system_error>

namespace visionrt {

namespace {

std::string pathText(const std::filesystem::path& path) {
    const auto text = path.generic_u8string();
    return {reinterpret_cast<const char*>(text.data()), text.size()};
}

std::filesystem::path utf8Path(std::string_view text) {
    const auto view = std::u8string_view(reinterpret_cast<const char8_t*>(text.data()), text.size());
    return std::filesystem::path(view);
}

bool cppKind(ToolKind kind) {
    return kind == ToolKind::CppFunction || kind == ToolKind::CppClass;
}

bool underRoot(const std::filesystem::path& root, const std::filesystem::path& path) {
    auto rootPart = root.begin();
    auto pathPart = path.begin();
    for (; rootPart != root.end(); ++rootPart, ++pathPart) {
        if (pathPart == path.end() || *pathPart != *rootPart) return false;
    }
    return true;
}

// Declarations under assets/tools/builtin are shipped platform tools; the rest of the package is custom tooling.
bool builtinDeclaration(const std::filesystem::path& relativePath) {
    static const std::filesystem::path builtinRoot = std::filesystem::path(u8"assets/tools/builtin");
    return underRoot(builtinRoot, relativePath.lexically_normal());
}

} // namespace

ToolsPackage readToolsPackage(const std::filesystem::path& packageRoot) {
    std::error_code error;
    if (!std::filesystem::is_directory(packageRoot, error) || error) {
        throw ContractError("Package root is not a valid directory: " + pathText(packageRoot));
    }

    ToolsPackage package;
    package.root = std::filesystem::absolute(packageRoot).lexically_normal();

    const auto indexFile = package.root / "module_index.yaml";
    if (!std::filesystem::is_regular_file(indexFile, error) || error) {
        throw ContractError("Missing module_index.yaml in package: " + pathText(package.root));
    }
    package.index = parseModuleIndexFile(indexFile);

    for (const auto& [moduleName, moduleEntry] : package.index.modules) {
        const auto library = (package.root / utf8Path(moduleEntry.library)).lexically_normal();
        if (!std::filesystem::is_regular_file(library, error) || error) {
            throw ContractError("Module '" + moduleName + "' library file does not exist: " + pathText(library));
        }
    }

    std::set<std::string> toolIds;
    package.tools.reserve(package.index.tools.size());
    for (const auto& [toolId, toolEntry] : package.index.tools) {
        PackageTool tool;
        tool.toolId = toolId;
        const auto definitionPath = (package.root / utf8Path(toolEntry.definition)).lexically_normal();
        if (!std::filesystem::is_regular_file(definitionPath, error) || error) {
            throw ContractError("Tool '" + toolId + "' definition file does not exist: " + pathText(definitionPath));
        }
        tool.definition = parseToolDefinitionFile(definitionPath, builtinDeclaration(utf8Path(toolEntry.definition)));
        if (tool.definition.id != toolId) {
            throw ContractError("Tool definition id '" + tool.definition.id +
                                "' does not match index tool_id '" + toolId + "'");
        }
        tool.registryName = tool.definition.registryName;
        tool.hostBinding = toolEntry.hostBinding;
        if (!toolIds.insert(tool.toolId).second) {
            throw ContractError("Duplicate tool_id in package: '" + tool.toolId + "'");
        }

        if (!tool.hostBinding.empty()) {
            if (!toolEntry.module.empty()) {
                throw ContractError("Tool '" + toolId + "' cannot specify both module and host_binding");
            }
            if (tool.definition.kind == ToolKind::PythonFunction || tool.definition.kind == ToolKind::PythonClass) {
                throw ContractError("Python tool '" + toolId + "' cannot specify host_binding");
            }
        } else if (cppKind(tool.definition.kind)) {
            if (toolEntry.module.empty()) {
                throw ContractError("C++ domain tool '" + toolId + "' must reference a module");
            }
            tool.moduleLibrary = (package.root /
                utf8Path(package.index.modules.at(toolEntry.module).library)).lexically_normal();
        } else if (tool.definition.kind == ToolKind::PythonFunction || tool.definition.kind == ToolKind::PythonClass) {
            if (!toolEntry.module.empty()) {
                throw ContractError("Python tool '" + toolId + "' cannot reference a module");
            }
            if (tool.definition.script.empty()) {
                throw ContractError("Python tool '" + toolId + "' declaration missing script entry");
            }
            const auto scriptPath = (definitionPath.parent_path() / tool.definition.script).lexically_normal();
            if (!std::filesystem::is_regular_file(scriptPath, error) || error) {
                throw ContractError("Python tool '" + toolId + "' script file does not exist: " + pathText(scriptPath));
            }
        }

        package.tools.push_back(std::move(tool));
    }

    std::sort(package.tools.begin(), package.tools.end(), [](const PackageTool& a, const PackageTool& b) {
        return a.definition.definitionPath.generic_u8string() < b.definition.definitionPath.generic_u8string();
    });

    for (const auto& runtimeFile : package.index.runtimeFiles) {
        const auto resolved = (package.root / utf8Path(runtimeFile)).lexically_normal();
        if (!std::filesystem::is_regular_file(resolved, error) || error) {
            throw ContractError("runtime_files entry does not exist: " + pathText(resolved));
        }
    }

    return package;
}

void validateToolsPackage(const std::filesystem::path& packageRoot) {
    readToolsPackage(packageRoot);
}

} // namespace visionrt
