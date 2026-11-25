#pragma once

// The tool declaration model every consumer shares: the host workbench, the packaging validator
// and an installed SDK consumer all read one schema into these types. Nothing here mentions a JSON
// library or a GUI framework, so a tool author can link the SDK without Qt.

#include "visionrt/contracts/Export.h"
#include "visionrt/contracts/ProtocolEnums.h"
#include "visionrt/contracts/Tool.h"
#include "visionrt/contracts/TypeSpec.h"
#include "visionrt/contracts/Value.h"

#include <cstdint>
#include <concepts>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace visionrt {

inline constexpr int kToolDeclarationSchemaVersion = 4;

// A port reference is a stage plus a number: the same number may exist once per direction.
struct PortRef
{
    PortStage stage                             = PortStage::Inputs;
    PortId    id                                = 0;
    bool      operator==(const PortRef &) const = default;
};

// A dynamic output type names three initialisation parameters by reference.
struct PortTypeReference
{
    PortRef flowParameter;
    PortRef nodeParameter;
    PortRef portParameter;
    bool    operator==(const PortTypeReference &) const = default;
};

// One declared parameter. `id` is its identity inside the tool and stage; the vector order is only
// the display order, so moving a row never renumbers it. `nameEn` is an author note for readers and
// migration diffs — nothing looks a port up by it.
struct PortOption
{
    PortValue value;
    std::string label;
    PortOption(PortValue v, std::string text = {}) : value(std::move(v)), label(std::move(text)) {}
    template<class T> requires std::constructible_from<PortValue, T>
    PortOption(T &&v) : value(std::forward<T>(v)) {}
    bool operator==(const PortOption &other) const { return label == other.label && equalValues(value, other.value); }
};

struct PortDefinition
{
    PortId                           id = 0;
    std::string                      nameZh;
    TypeSpec                         type;
    // The declaration's explicit default, or the type's empty value materialised during
    // normalisation. A host-owned status port and a type_from output keep it Missing.
    PortValue                        defaultValue = Missing{};
    std::string                      nameEn;
    EditorKind                       editor = EditorKind::Default;
    ParameterTab                     tab    = ParameterTab::BasicInput;
    std::string                      group;
    std::optional<PortValue>         step;
    std::optional<int>               decimals;
    OptionsProvider                  optionsProvider = OptionsProvider::None;
    std::optional<PortValue>         minimum;
    std::optional<PortValue>         maximum;
    std::vector<PortOption>          options;
    std::optional<PortRef>           image;
    std::optional<PortRef>           alignment;
    std::optional<PortRef>           sameGeometryAs;
    ColorOrder                       colorOrder = ColorOrder::Automatic;
    // Author's explicit choice to offer this Image port in the workbench image selector. Absent in
    // the declaration means false: an image port never joins the selector without being named.
    bool                             imageVisible = false;
    std::optional<PortTypeReference> typeFrom;
    ProducedBy producedBy = ProducedBy::Both;
    int minRegions = 0;
    int maxRegions = 0;
    VT_EXPORT bool operator==(const PortDefinition &other) const;
};

// Model bytes are transient ToolResult artifacts; the declaration binds their identity to
// an editable training path and the shared path output.
struct TrainingArtifactSpec
{
    std::string id;
    PortId sourcePort = 0;
    std::string format;
    int version = 1;
    PortRef target{PortStage::Train, 0};
    bool operator==(const TrainingArtifactSpec&) const = default;
};

struct TrainingDatasetSpec
{
    PortId parameter = 0;
    std::optional<PortRef> image;
    int maxRecords = 0;
    std::vector<PortDefinition> fields;
    bool operator==(const TrainingDatasetSpec &) const = default;
};

struct TrainingSpec
{
    std::vector<TrainingDatasetSpec> datasets;
    std::vector<TrainingArtifactSpec> artifacts;
    bool operator==(const TrainingSpec&) const = default;
};

struct PythonEntry
{
    std::filesystem::path package;
    std::filesystem::path file;
    std::string           symbol;
    bool                  operator==(const PythonEntry &) const = default;
};

// Everything a declaration file states about a tool, with no host paths or runtime state. The host
// extends this with where the file was found, its tree category and the roots it resolves against;
// a packaging validator or a tool author holds the declaration alone.
struct ToolDeclaration
{
    std::string                        nameEn;
    std::string                        nameZh;
    std::string                        version    = "1.0.0";
    ToolKind                           kind       = ToolKind::Cpp;
    NodeModel                          nodeModel  = NodeModel::Fixed;
    ToolVisibility                     visibility = ToolVisibility::Normal;
    std::string                        hostBinding;
    PythonEntry                        entry;
    std::vector<std::filesystem::path> resources;
    std::optional<TrainingSpec>        training;
    std::vector<PortDefinition>        trainParams;
    std::vector<PortDefinition>        initParams;
    std::vector<PortDefinition>        inputs;
    std::vector<PortDefinition>        outputs;

    bool derivesPorts() const noexcept
    {
        return nodeModel != NodeModel::Fixed;
    }
};

// "inputs.7" — diagnostics only. The number is the identity; the text never becomes one.
VT_EXPORT std::string formatPortRef(const PortRef &reference);

// The declaration spelling of a stage ("init", "inputs", "outputs"), used by every message that has
// to name a port the author wrote.
VT_EXPORT const char *stageName(PortStage stage) noexcept;

// Canonical host-provided ports. Input 0 carries the upstream status; outputs 0 and 1 carry the
// status envelope. They are materialised by normalisation and never written by tool declarations.
VT_EXPORT bool isReservedPort(PortStage stage, PortId id) noexcept;

// The label, type and default of one host-owned port. Normalisation materialises a copy into a
// declaration and the editor projects the same copy as a fixed row, so the two cannot drift apart.
VT_EXPORT PortDefinition hostPort(PortStage stage, PortId id);

// Input 0 is reserved for every model, but only a Fixed tool is handed the upstream status.
VT_EXPORT bool isInjectedInput(NodeModel model, PortId id) noexcept;

// Initialisation entries derived from the node model rather than edited directly. They are still
// handed to the tool through nodeParameters(), but never become parameter rows.
inline constexpr PortId kVariablesInitPort = 0;
inline constexpr PortId kProgramInitPort   = 1;
inline constexpr PortId kConditionModePort = 2;
VT_EXPORT bool          isDerivedInitParam(NodeModel model, PortId id) noexcept;

} // namespace visionrt
