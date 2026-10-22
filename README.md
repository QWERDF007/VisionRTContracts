# VisionRTContracts

C++20 工具接口与 owned 值契约。公共接口见 [include/visionrt/contracts](include/visionrt/contracts)，源码与验证入口见 [文档索引](docs/README.md)。

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
