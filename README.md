# VisionRTContracts

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

父工程可通过 Git 子模块方式检入 `3rdparty/VisionRTContracts`，并使用：

```cmake
add_subdirectory(3rdparty/VisionRTContracts)
```

SDK 暴露以下标准入口：

- `VisionRT::Contracts`：静态库目标，已传递头文件路径与依赖
- `VisionRT::visionrt_validate_tool`：校验器命令行目标
- `VISIONRT_CONTRACTS_INCLUDE_DIR`：SDK 公共头文件根目录路径

### 可选 PythonInterop

核心 SDK 默认不依赖 Python。启用 `-DVISIONRT_BUILD_PYTHON_INTEROP=ON` 时需要匹配的 Python 嵌入开发环境及 pybind11 CMake 包，缺依赖时配置失败。`VisionRT::ContractsPython` 传递依赖和统一的 pybind11 编译定义；消费者不应再单独定义不同的 pybind11 ABI 选项。

```cmake
find_package(VisionRTContracts CONFIG REQUIRED)
target_link_libraries(consumer PRIVATE VisionRT::ContractsPython)
```

使用安装包时通过 `CMAKE_PREFIX_PATH`、`pybind11_DIR`、`Python3_ROOT_DIR` 指定依赖位置。解释器创建、GIL 和实例生命周期由消费者负责，转换边界见 [PythonInterop.h](include/visionrt/contracts/PythonInterop.h)。

[接口与验证索引](docs/README.md)
