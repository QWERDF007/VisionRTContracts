# SDK 文档索引

- [构建与安装消费](../README.md)
- [工具声明与显式资源](../include/visionrt/contracts/Schema.h)、[YAML 解析](../src/SchemaYaml.cpp)
- [源端口类型引用与声明校验](../src/Schema.cpp)、[动态输出包合同回归](../tests/test_sdk_package_contract.cpp)
- [强类型工具包](../include/visionrt/contracts/Package.h)、[包读取与路径检查](../src/Package.cpp)
- [值与所有权合同](../include/visionrt/contracts/Value.h)
- [Rect／路径引用／YAML 契约](../tests/test_sdk_value_contract.cpp)、[Python 只读矩形与生命周期](../tests/test_rect_interop.cpp)、[模块 ABI](../include/visionrt/contracts/Module.h)
- [可选 Python 转换接口](../include/visionrt/contracts/PythonInterop.h)、[实现](../src/PythonInterop.cpp)
- [CMake 可选依赖与导出](../CMakeLists.txt)、[安装配置](../cmake/VisionRTContractsConfig.cmake.in)
- [隔离安装驱动](../tests/test_isolated_installation.py)、[独立 CMake 消费工程](../tests/installed_consumer/CMakeLists.txt)、[消费契约](../tests/installed_consumer/main.cpp)：核心和可选桥接分别消费真实安装结果。
- [包合同回归](../tests/test_sdk_package_contract.cpp)、[SDK 消费语料](../tests/test_sdk_consumer.cpp)

包结构与数据转换语义以公共接口和实现为准，不在文档复制第二份规则。
