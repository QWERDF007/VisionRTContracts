# VisionRTContracts

C++20 工具接口与共享可写值契约。公共接口见 [include/visionrt/contracts](include/visionrt/contracts)，源码与验证入口见 [文档索引](docs/README.md)。

## 状态与端口归属

- 工具返回 `ToolResult{outputs, status}`：`outputs` 只含声明里的业务端口，`status` 是本次执行的唯一结论。
- `__status_code` / `__status_message` 由宿主在工具正常返回后物化，工具不写这两个键。
- `InputReader` / `OutputWriter` / `PathResolver` 对可预期的坏输入给 `PortIssue`，不抛异常；业务码由工具自己决定。
- 模块 ABI 6 起 `ToolResult` 布局如上，5 不再兼容。

## 构建与安装

依赖 CMake 3.25+、C++20 编译器和 OpenCV core。测试驱动另需 Python 解释器。

```sh
cmake -S . -B build -DOpenCV_DIR=<OpenCVConfig.cmake目录> -DBUILD_TESTING=OFF
cmake --build build --config Release
cmake --install build --config Release --prefix <安装目录>
```

消费方通过 `CMAKE_PREFIX_PATH` 指定安装目录和 OpenCV：

```cmake
find_package(VisionRTContracts CONFIG REQUIRED)
target_link_libraries(consumer PRIVATE VisionRT::Contracts)
```

工具声明、包读取、校验 CLI 和 Python 转换属于宿主，不是 SDK 接口。
