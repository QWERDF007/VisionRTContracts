#pragma once

// The one declaration reader and writer. UTF-8 JSON in, declaration out; declaration in, UTF-8
// JSON out. Both directions are strict: an unknown field, a reserved port, a number that is not an
// exact integer of the declared type, or a value that does not fit its port type is rejected with
// the JSON path of the field that caused it. Diagnostics carry no library-specific text, so an
// author running the SDK and the workbench loading the same file read the same sentence.

#include "visionrt/contracts/Declaration.h"
#include "visionrt/contracts/Export.h"

#include <filesystem>
#include <string>
#include <string_view>

namespace visionrt {

// Reads one declaration document. `source` names the document in diagnostics as UTF-8; pass an
// empty name when the text came from somewhere that has no path. The name is text, never a path
// the reader opens, so a non-ASCII name cannot fail on a code page.
VT_EXPORT ToolDeclaration readToolDeclaration(std::string_view json, std::string_view source = {});

// Reads the file as UTF-8. I/O failures name the path and never fall back to an empty declaration.
VT_EXPORT ToolDeclaration readToolDeclarationFile(const std::filesystem::path &path);

// Emits the declaration a workbench loads: reserved host ports are omitted, a default equal to the
// type's empty value is omitted, and fields left at their default value are left out. The result is
// stable for a given declaration, so a save/reopen cycle does not churn the file.
VT_EXPORT std::string writeToolDeclaration(const ToolDeclaration &declaration);

} // namespace visionrt
