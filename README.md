# VisionRTToolSDK

VisionRT 与 VisionTools 共享的 Tool SDK 核心静态库与校验器工具。

## 负责范围

- 共享值模型与类型规范（`Value.h`、`Contract.h`）
- 算子声明模型与模块契约（`Schema.h`、`Tool.h`、`Module.h`）
- YAML 序列化、反序列化与严格契约校验（`ValueYaml.h`、`SchemaYaml.h`）
- 模块契约与工具包完整性校验器（`visionrt_validate_tool`）

## 构建与测试

### 环境依赖

- CMake >= 3.25
- C++20 兼容编译器（如 MSVC 2022）
- OpenCV 4.x (`core` 模块)
- yaml-cpp（本仓库内嵌 `3rdparty/yaml-cpp` 子模块）

### 检出依赖

```bash
git submodule update --init --recursive
```

### 构建与测试

使用提供的构建脚本：

```cmd
tools\build.cmd --clean --test
```

或使用 CMake 独立构建：

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build --output-on-failure -C Release
```

### 供父项目集成

父工程可通过 Git 子模块方式检入 `3rdparty/VisionRTToolSDK`，并使用：

```cmake
add_subdirectory(3rdparty/VisionRTToolSDK)
```

SDK 暴露以下标准入口：

- `VisionRT::ToolSDK`：静态库目标，已传递头文件路径与依赖
- `VisionRT::visionrt_validate_tool`：校验器命令行目标
- `VISIONRT_TOOL_SDK_INCLUDE_DIR`：SDK 公共头文件根目录路径
