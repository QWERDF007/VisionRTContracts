#pragma once

namespace visionrt {
inline constexpr int kTrainingAssetVersion = 2;
enum class ToolKind { Cpp = 0, PythonFunction = 1, PythonClass = 2, Host = 3 };
enum class BindingMode { Literal = 0, Upstream = 1 };
enum class PortStage { Init = 0, Inputs = 1, Outputs = 2 };
enum class NodeModel { Fixed = 0, Expression = 1, Condition = 2, Select = 3 };
enum class ToolVisibility { Normal = 0, Assist = 1 };
enum class ToolOrigin { Builtin = 0, Custom = 1 };
enum class TrainingKind { ColorSamples = 0, StraightEdges = 1 };
enum class EditorKind {
    Default = 0, Number = 1, Text = 2, Bool = 3, Switch = 4, Recursive = 5,
    Path = 6, Array = 7, Roi = 8, Rect = 9, Condition = 10, ConditionUpstream = 11,
    Expression = 12, Select = 13
};
enum class ParameterTab { BasicInput = 0, Preprocess = 1, Detection = 2, Postprocess = 3 };
enum class OptionsProvider { None = 0, GpuDevices = 1 };
enum class ColorOrder { Automatic = 0, Gray = 1, BGR = 2, BGRA = 3, RGB = 4, RGBA = 5 };
}
