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
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace visionrt {

// A port reference is a stage plus a number: the same number may exist once per direction.
struct PortRef
{
    PortStage stage = PortStage::Inputs;
    PortId    id    = 0;
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
struct PortDefinition
{
    PortId      id = 0;
    std::string nameZh;
    TypeSpec    type;
    // The declaration's explicit default, or the type's empty value materialised during
    // normalisation. A host-owned status port and a type_from output keep it Missing.
    PortValue   defaultValue = Missing{};
    std::string nameEn;
    EditorKind  editor         = EditorKind::Default;
    ParameterTab tab           = ParameterTab::BasicInput;
    std::string group;
    std::optional<PortValue> step;
    std::optional<int>       decimals;
    OptionsProvider          optionsProvider = OptionsProvider::None;
    std::optional<PortValue> minimum;
    std::optional<PortValue> maximum;
    std::vector<PortValue>   options;
    std::optional<PortRef>   image;
    std::optional<PortRef>   alignment;
    std::optional<PortRef>   sameGeometryAs;
    ColorOrder               colorOrder = ColorOrder::Automatic;
    // Author's explicit choice to offer this Image port in the workbench image selector. Absent in
    // the declaration means false: an image port never joins the selector without being named.
    bool                             imageVisible = false;
    std::optional<PortTypeReference> typeFrom;
};

// Helper port number -> the tool port it feeds (inputs) or proposes into (proposals).
struct InteractionMapping
{
    std::string               helperNameEn;
    std::map<PortId, PortRef> inputs;
    std::map<PortId, PortRef> proposals;
    std::vector<RoiKind>      roiKinds;
    bool                      operator==(const InteractionMapping &) const = default;
};

// One typed field a training record carries. `id` is the number the trainer receives and the
// record is keyed by it; `nameZh` is presentation only, so renaming a field never moves its value.
// `roi` marks the one field the canvas edits, and `minRegions`/`maxRegions` the geometry budget
// that field accepts (0 means unconstrained).
struct TrainingFieldSpec
{
    PortId      id = 0;
    std::string nameZh;
    TypeSpec    type;
    bool        roi        = false;
    int         minRegions = 0;
    int         maxRegions = 0;
    bool        operator==(const TrainingFieldSpec &) const = default;
};

// One value the training page offers beside the records. The trainer reads it through a call input
// that names this number, and a declared write publishes the fitted value back into the node; the
// declaration states the default the field starts from, never a second copy in the host.
struct TrainingParameterSpec
{
    PortId                   id = 0;
    std::string              nameZh;
    TypeSpec                 type;
    PortValue                defaultValue = Missing{};
    EditorKind               editor       = EditorKind::Default;
    std::optional<PortValue> step;
    std::optional<int>       decimals;
    std::optional<PortValue> minimum;
    std::optional<PortValue> maximum;
    std::vector<PortValue>   options;
    // The declared default is a value, and a value has no equality of its own: what makes a
    // declaration the same is its identity, type and control, not the constant it starts from.
    bool sameShape(const TrainingParameterSpec &other) const
    {
        return id == other.id && nameZh == other.nameZh && type == other.type && editor == other.editor
               && decimals == other.decimals;
    }
};

// One model the trainer returns. `format` names the tool-side encoding the host never interprets;
// `target` is the initialization parameter the published file path is derived into.
struct TrainingArtifactSpec
{
    std::string id;
    PortId      sourcePort = 0;
    std::string format;
    int         version = 1;
    PortRef     target;
    bool        operator==(const TrainingArtifactSpec &) const = default;
};

// A call input that hands the trainer the number of a declared record field, so the trainer learns
// which fields to read without the host guessing one by its type.
struct TrainingFieldRef
{
    PortId id = 0;
    bool   operator==(const TrainingFieldRef &) const = default;
};

// A call input that hands the trainer the current value of a declared training parameter.
struct TrainingParameterRef
{
    PortId id = 0;
    bool   operator==(const TrainingParameterRef &) const = default;
};

// The record shape a training draft follows. `maxCount` of 0 leaves the number of records to the
// user; a tool that needs one reference record states it here instead of the host remembering
// the tool's name.
struct TrainingRecordsSpec
{
    int                            maxCount = 0;
    std::vector<TrainingFieldSpec> fields;

    // The field the canvas edits, or 0 when the tool declares no geometry field.
    PortId roiField() const noexcept
    {
        for (const auto &field : fields)
            if (field.roi)
                return field.id;
        return 0;
    }
    bool operator==(const TrainingRecordsSpec &) const = default;
};

// One helper output written to one workbench target. The list is complete: a click writes exactly
// these targets or nothing.
struct AuthoringWrite
{
    PortId  sourcePort = 0;
    PortRef target;
    bool    operator==(const AuthoringWrite &) const = default;
};

// Facts only the workbench holds at the moment of a gesture. A helper input either takes one of
// them or the value of a node port the declaration names; a constant belongs to the helper's own
// declared default, so the business declaration never restates it.
enum class ContextRole
{
    Cursor        = 0,
    Phase         = 1,
    Append        = 2,
    TrainingImage = 3,
    TrainingSet   = 4
};

// A typed constant a call passes to the helper. The declaration states the type the helper port
// requires, so the value survives a round trip and the host can verify it against the real port
// before the call instead of guessing from the JSON token.
struct AuthoringLiteral
{
    TypeSpec  type;
    PortValue value = Missing{};

    // Literals are scalars: the declaration states one constant, not a picture or a region. Equality
    // therefore compares the number, text or flag the declaration spells.
    bool operator==(const AuthoringLiteral &other) const
    {
        if (type != other.type || value.index() != other.value.index())
            return false;
        if (const auto *number = std::get_if<std::int64_t>(&value))
            return *number == std::get<std::int64_t>(other.value);
        if (const auto *number = std::get_if<double>(&value))
            return *number == std::get<double>(other.value);
        if (const auto *flag = std::get_if<bool>(&value))
            return *flag == std::get<bool>(other.value);
        if (const auto *text = std::get_if<std::string>(&value))
            return *text == std::get<std::string>(other.value);
        return false;
    }
};

// One helper input: the helper port it feeds and where the value comes from. A source port of type
// Image is read as the frame the user is looking at, not as the port's stored literal. A training
// call additionally names record field numbers, the current training parameter values and typed
// literals the declaration states itself.
struct AuthoringInput
{
    PortId port = 0;
    std::variant<ContextRole, PortRef, AuthoringLiteral, TrainingFieldRef, TrainingParameterRef> source;
    bool operator==(const AuthoringInput &) const = default;
};

// The helper a scope calls. Ports the call omits keep the helper's own declared default.
struct AuthoringCall
{
    std::string                 tool;
    std::vector<AuthoringInput> inputs;
    bool operator==(const AuthoringCall &) const = default;
};

// What a tool can be trained on. The host owns the training draft; the declaration names the
// frame "加入训练" copies, the fields each record carries, the parameters the training page offers
// and the trainer that turns the draft into node writes and model assets. A tool without this
// block has no training UI at all.
struct TrainingSpec
{
    TrainingKind kind       = TrainingKind::ColorSamples;
    PortId       assetParam = 0;
    // Assistant that turns one click on the picture into a record geometry, named by the same
    // mechanism as a tool interaction helper. Empty means the session is edited by hand only.
    std::string helper;
    // Port whose published frame is copied into the draft. `id` of 0 means the tool never adds a
    // frame, which is only valid when it also declares no record fields.
    PortRef            image;
    TrainingRecordsSpec records;
    // The trainer applied to the draft. Empty means the tool keeps its pre-training data format
    // and the host writes that format directly.
    AuthoringCall                  call;
    std::vector<TrainingParameterSpec> parameters;
    // Trainer output -> workbench target, written in the same transaction that publishes the asset.
    std::vector<AuthoringWrite>        writes;
    std::vector<TrainingArtifactSpec>  artifacts;

    bool operator==(const TrainingSpec &other) const
    {
        if (kind != other.kind || assetParam != other.assetParam || helper != other.helper
            || image != other.image || records != other.records || call != other.call
            || writes != other.writes || artifacts != other.artifacts
            || parameters.size() != other.parameters.size())
            return false;
        for (std::size_t index = 0; index < parameters.size(); ++index)
            if (!parameters[index].sameShape(other.parameters[index]))
                return false;
        return true;
    }
};

// Point selection on the processing page. `image` is the picture the user points at, `preview` and
// `valid` name the helper outputs the canvas and the click decision read, `message` is optional.
struct EasyTouchSpec
{
    PortRef                     image;
    AuthoringCall               call;
    PortId                      previewOutput = 0;
    PortId                      validOutput   = 0;
    std::optional<PortId>       messageOutput;
    std::vector<AuthoringWrite> writes;
    bool operator==(const EasyTouchSpec &) const = default;
};

// The optional capability block. Omitting it is an ordinary tool: no point selection, no training.
struct AuthoringSpec
{
    std::uint32_t                version = 1;
    std::optional<EasyTouchSpec> easyTouch;
    bool operator==(const AuthoringSpec &) const = default;
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
    std::string                       nameEn;
    std::string                       nameZh;
    std::string                       version    = "1.0.0";
    ToolKind                          kind       = ToolKind::Cpp;
    NodeModel                         nodeModel  = NodeModel::Fixed;
    ToolVisibility                    visibility = ToolVisibility::Normal;
    std::string                       hostBinding;
    PythonEntry                       entry;
    std::vector<std::filesystem::path> resources;
    std::optional<InteractionMapping> interaction;
    std::optional<TrainingSpec>       training;
    std::optional<AuthoringSpec>      authoring;
    std::vector<PortDefinition>       initParams;
    std::vector<PortDefinition>       inputs;
    std::vector<PortDefinition>       outputs;

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
