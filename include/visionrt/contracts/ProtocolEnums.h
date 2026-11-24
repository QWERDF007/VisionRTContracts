#pragma once

namespace visionrt {
// A published training asset: typed records, parameters, lossless source frames and model files.
inline constexpr int kTrainingManifestVersion = 4;
enum class ToolKind { Cpp = 0, PythonFunction = 1, PythonClass = 2, Host = 3 };
enum class BindingMode { Literal = 0, Upstream = 1 };
enum class PortStage { Init = 0, Inputs = 1, Outputs = 2, Train = 3 };
enum class NodeModel { Fixed = 0, Expression = 1, Condition = 2, Select = 3 };
enum class ToolVisibility
{
    Normal   = 0,
    Internal = 1
};
enum class ToolOrigin { Builtin = 0, Custom = 1 };
enum class EditorKind {
    Default = 0, Number = 1, Text = 2, Bool = 3, Switch = 4, Recursive = 5,
    Path = 6, Array = 7, Roi = 8, Rect = 9, Condition = 10, ConditionUpstream = 11,
    Expression = 12, Select = 13
};
enum class ParameterTab { BasicInput = 0, Preprocess = 1, Training = 2, Detection = 3, Postprocess = 4 };
enum class ParameterScope { Tool = 0, Record = 1 };
enum class ProducedBy { Both = 0, Execute = 1, Train = 2 };

enum class OptionsProvider { None = 0, GpuDevices = 1 };
enum class ColorOrder { Automatic = 0, Gray = 1, BGR = 2, BGRA = 3, RGB = 4, RGBA = 5 };
}
