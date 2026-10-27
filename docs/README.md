# 契约文档索引

- [构建与安装消费](../README.md)、[CMake 导出](../CMakeLists.txt)、[安装配置](../cmake/VisionRTContractsConfig.cmake.in)
- [值与结构校验](../include/visionrt/contracts/Value.h)、[实现](../src/Value.cpp)
- [端口类型](../include/visionrt/contracts/TypeSpec.h)、[嵌套数组与运行时能力](../include/visionrt/contracts/Array.h)、[ROI 几何](../include/visionrt/contracts/ROI.h)
- [共享可写数据与依赖保活](../include/visionrt/contracts/DataRef.h)、[生命周期契约](../tests/test_data_ref_contract.cpp)
- [工具实例与状态](../include/visionrt/contracts/Tool.h)、[工厂注册接口](../include/visionrt/contracts/Registry.h)、[模块 ABI](../include/visionrt/contracts/Module.h)
- [值契约测试](../tests/test_sdk_value_contract.cpp)
- [隔离安装驱动](../tests/test_isolated_installation.py)、[独立消费工程](../tests/installed_consumer/CMakeLists.txt)、[消费契约](../tests/installed_consumer/main.cpp)
