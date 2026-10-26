# WORKLOG

> 本文件是本 SDK 仓库唯一任务账本；真实日志位于固定示例之后，最新在前。

## ⏳ 待你裁决

（暂无待裁决事项）

## 日志

<!--
建议格式：
### YYYY-MM-DD — 简短任务名
**目标**
- ...
**当前状态**
- 已完成与未完成项
**验证证据**
- 命令与结果；未验证项明确说明
**下一步**
- ...
-->

### 2026-10-23 — 隔离消费校验无宿主依赖

**目标**
- 为B01/B03/N05把“SDK消费者不得依赖Qt/YAML/Python/CUDA/InferRT”固化为隔离安装测试断言，而不是一次人工核验。

**当前状态**
- `test_isolated_installation.py` 在启动consumer前读取动态依赖：Windows从`--cxx-compiler`同目录定位dumpbin，macOS用otool，Linux用ldd；覆盖consumer可执行文件及同目录全部本地DLL/SO；命中Qt/Qt5/YAML/Python/CUDA/InferRT/TensorRT即失败。dumpbin不可用时明确打印跳过，不静默通过。
- SDK与consumer源码未改。

**验证证据**
- Contracts 4/4通过；`test_isolated_installation` 10.57秒，依赖检查实际执行（无跳过提示）。
- 提交c4091f131e51bdf3f224246a8acd9bdf17113e9a已推送；宿主gitlink更新为29f2122。

**下一步**
- 继续三仓发行/实机复核；Linux仍not_run。

### 2026-10-23 — 隔离安装消费遵循父生成器

**目标**
- 修复 `test_isolated_installation` 在 Visual Studio 父构建下强制 `-G Ninja` 却传入 MSBuild make program 导致的配置失败，使真实安装消费测试可随父工程生成器运行。

**当前状态**
- 测试脚本新增 `--generator`/`--generator-platform`，按父生成器配置 consumer；Visual Studio 生成器不传 Ninja make program、编译器与单配置 build type，Ninja 仍保留原参数。CMake 注册传入 `${CMAKE_GENERATOR}` 与平台。
- SDK 未改；仅测试 harness 修复，隔离安装仍验证安装头/Config/独立 consumer 构建与运行。

**验证证据**
- 新 build-round14：Contracts 4/4 通过，`test_isolated_installation` 11.50 秒；`test_sdk_value_contract`/`test_data_ref_contract`/`test_array_contract` 均通过。
- 提交 46f6d024fb59a9e184ee505b44042bfea8d2f5b0 已推送 origin/refactor/abi4-writable-data；宿主 gitlink 更新为 3a5b0ae。

**下一步**
- 继续 UCT/三仓发行与实机复核；WORKLOG 未提交。

### 2026-10-22 — 统一公开值与 ABI4

**目标**
- 按新方案 UCT-05 将旧只读载体整体替换为真实可写 Image/Array/ROI，切换 ABI4，不保留兼容类型。

**当前状态**
- TypeSpec.h 统一七种类型与 Array dtype/rank；Array.h 提供任意模板层数 NestedVector，并集中预编译七种 dtype、一至四维的实际 DataRef<vector> 分派。Value.h 的复杂值共享句柄，String 为 std::string；删除旧 Utf8/BufferOwner/OwnedImage/NumericArray/StringArray/ImageBatch/RoiBatch/Roi/Rect/Layout 及其实现。
- ROI.h/ROI.cpp 提供三层 double 载体与纯几何函数，补 code 0 点；可写载体的结构在 validateValue 边界重新校验。retainValue 为复杂值添加外层依赖，普通配置比较按数组/ROI 内容进行，图像只比较视图身份。
- SDK MODULE_ABI_MAJOR=4；删除无引用 tools/dependencies.yaml 和构建脚本旧机器路径，沿用 CMake 依赖发现。文档仅更新类型/几何索引。
- SDK 已完成本次定向验证，尚未关闭整个 UCT-05/06：宿主 JSON/Python/插件模块保活、Tools 源码、真实卸载与 Python 退出仍待配套迁移；不得混用 ABI3 模块。三仓整体交付尚未完成。
- 已提交并推送 refactor/abi4-writable-data：5d91ae718a16b0fea1d9b1ede85662f452587ab6；origin 同名分支经 ls-remote 核验一致，见宿主 build/abi4-sdk-commit.log、abi4-sdk-push.log。WORKLOG 未暂存/提交，main 未改。父仓指针等待其配套源码迁移验证后提交。

**验证证据**
- 宿主 build/array-tracer-red.log：公开 Array.h 缺失的真实编译 RED；array-tracer-green-* 为初始 GREEN。初次绿色编译暴露 assert 宏对模板逗号的解析，改为测试本地类型别名，没有改产品 API 兜底。
- build/abi4-sdk-final-build.log、abi4-sdk-final-tests.log：SDK 值、DataRef、Array 定向 3/3；覆盖七种 dtype × 四个 rank 的非空 ragged 容器/别名写入、28 种保活释放计数、vector<bool>、double ROI、UTF-8、精确 int64 和错误状态。
- build/abi4-sdk-final-installed.log：真实干净安装与 SHARED DLL 消费 1/1，4.42 秒；DLL 中生产 Image、Array、ROI，消费者修改共享数据并确认 double 精度及外部图像释放次数。不是实际 DLL 卸载或 Python 生命周期验证。
- clang-format 已用于改动/新增 C++ 代码；git diff --check 通过；公开头及实现扫描无旧载体/布局 API。提交继续排除 WORKLOG。

**下一步**
- 将 Tools 内置工具改为 ABI4，使用同一 Array 分派与 Image/ROI DataRef；再配套迁移宿主 ValueJson/DefinitionJson/Python。宿主 gitlink 只在其源码完成配套后切换，不发布半套兼容层。

### 2026-10-22 — 新方案 DataRef 基础闭环

**目标**
- 以宿主 docs/VisionRT_统一完整方案_Specs_Tickets-新.zip 为唯一规格入口，按用户确认的顺序实施 UCT-05/06；本闭环先提供可写共享句柄。

**当前状态**
- 分支 refactor/abi4-writable-data，起点 a9336dd；新增 DataRef.h 和公开生命周期测试，安装消费者通过真实 SHARED DLL 生产/修改外部缓冲图像。句柄提供 get/owner/retainedBy，不增加读写属性或自动复制。
- 所有权状态将 payload 与依赖保持在同一共享生命周期；外层依赖覆盖内层 payload 和释放回调返回，默认句柄替换不提前丢弃旧依赖。文档只维护实现/测试索引。
- 尚未改变 Value.h、TypeSpec 或 ABI3；这些旧载体必须在后续 UCT-05 中整体替换，不作为兼容保留。真实模块卸载、Python owner/GIL/解释器退出尚未验证，M1 未关闭。
- 已提交并推送 refactor/abi4-writable-data：5bc98e91d4af6d7c1b422720bb526223fcab5a3c；父仓 Tools 0546eaa、宿主 f51419d 已同步并推送，dev/main 未改。WORKLOG 仅本地维护，未提交。远程 SHA 证据见宿主 build/data-ref-checkpoint-push.log。

**验证证据**
- 宿主 build/data-ref-sharing-red.log、data-ref-dependency-red.log：缺失接口编译 RED；相应 green 日志通过。
- 宿主 build/data-ref-final-tests.log：SDK 值及 DataRef 两项通过；data-ref-final-installed.log：真实独立安装/SHARED 消费通过，2.59 秒。公开测试包含共享地址/可写别名/显式 clone、析构计数、工作线程最终释放、复制/移动替换与自赋值。

**下一步**
- 继续统一 Array/ROI/Image 与 TypeSpec，最终切换 ABI4 并删除旧模型，随后再关闭 UCT-05/06；后续提交继续明确排除 WORKLOG。

### 2026-10-22 — 实施 CT-01～CT-05 共享契约能力

**目标**
- 按 `docs/VisionRT_完整实施方案_Specs_Tickets.zip` 落地 Contracts 侧 P1/P2 工单：路径分类、执行状态规范化、实际值约束、规范化 YAML、独立安装与 PIC。

**当前状态**
- 已完成 CT-01：`isPathPort` 统一 file/directory/path 分类，Schema 拒绝非 String 路径编辑器；固定四页由 `toolTabs()` 导出。
- 已完成 CT-02：`executionSuccess`/`normalizeExecutionStatus` 统一空消息、中文失败与 -5 取消；`ToolStatus` 默认消息保持为空。
- 已完成 CT-03：`PortContract` 承载 options/min/max，`portContractFromDefinition` 从声明生成执行合同，`validateToolValues` 统一类型、Missing、选项、范围、有限数值与 int64 精度，原因含端口 ID。
- 已完成 CT-04：`emitNormalizedToolYaml` 输出完整端口硬约束、相对 entry/resources 与稳定键序；CLI 新增 `--emit-normalized-yaml`。
- 已完成 CT-05 的安装与 PIC 部分：移除 OpenCV 默认路径 YAML 扫描，Contracts/PythonInterop 目标级 PIC，安装消费者改用 `VisionRT::Contracts`/`ContractsPython` 并新增真实 SHARED 模块。可见英文合同错误中文化尚未完成。

**验证证据**
- `tools\build.cmd cmake --build 3rdparty\VisionRTContracts\build-dev` → 成功。
- `tools\build.cmd ctest --test-dir 3rdparty\VisionRTContracts\build-dev --output-on-failure` → 6/6 Passed，含新增 `test_sdk_runtime_contract`、扩展 `test_cli` 与 `test_isolated_installation`（独立安装 + SHARED 消费）。

**下一步**
- 完成可见合同错误中文化；随后更新宿主/工具库子模块指针并实施 VT/RT 工单。

### 2026-10-21 — Rect 的隔离安装消费

**目标**
- 验证 ABI 2 与 Rect 在核心 SDK、可选 PythonInterop 的实际安装结果中可被独立工程消费。

**当前状态**
- 既有安装驱动改为构建 tests/installed_consumer 中的真实 CMake／C++ 夹具，删除 Python 内嵌的两份 C++／CMake 字符串。两种配置共用 Rect 类型、Schema 图像关联、YAML、Missing、非法几何和精确整数合同；ON 另验证只读共享 Rect、胶囊生命周期、非连续图像步长、返回值复制、解释器退出后的 C++ 所有权及 dtype／形状拒绝。
- 通过实际安装目录的 Config 查找，关闭用户／系统 package registry；Windows 消费进程 PATH 只保留系统及显式 OpenCV／Python 依赖，清空 PYTHONPATH。不修改产品转换实现、模块 ABI 或工具包格式。
- 测试、夹具与索引同步到 SDK 主检出及宿主／工具库两处子模块；不提交 WORKLOG，整体应用交互验收仍未完成。

**验证证据**
- F:/Projects/VisionRT/build/sdk-rect-clean-off-test.log：核心配置 test_isolated_installation 1/1、2.69 秒，真实安装、独立配置编译并执行 ABI 2／Rect Schema/YAML／Int64 消费者。
- F:/Projects/VisionRT/build/sdk-rect-clean-on-test.log：PythonInterop 配置 test_isolated_installation 1/1、5.90 秒，使用 Python 3.12.13、pybind11 3.2.0；包含解释器退出后读取 C++ 结果的断言。另有 sdk-rect-install-on-contracts.log 值与 Rect Python 2/2、0.18 秒；OFF 的 sdk-rect-install-off-contracts.log 1/1。未运行仓库全量测试。
- ON 首次配置因既有缓存仍指向 Python 3.9 头／库而拒绝 Development.Embed；cmake --fresh 在同一 build/sdk-delivery-on 重配后，解释器／include／library 都指向 py312。夹具中状态端口下标和 Python accessor 的整数零索引问题已修正，不将夹具错误当作 SDK 产品缺陷或业务红灯。

**下一步**
- 以 SDK 及安装消费契约为准继续 VisionTools 完整配置、宿主 ROI／绑定和真实安装交互验收；详细 API、测试与导出入口见 docs/README.md，不在账本维护第二套接口说明。

### 2026-10-21 — 路径模式引用与消费验证

**目标**
- 工具以元数据引用路径模式参数，宿主不按工具 ID 选择文件／目录；复核 Rect 与 ABI 边界。

**当前状态**
- Schema／SchemaYaml 支持 path_mode_from，限定为同一参数作用域内的 String 引用；值、类型、Schema、Python 及测试共 12 份源码在主仓和两处子模块一致，未提交。
- 宿主已消费路径引用、无图 Rect 数值编辑和新 ABI 工具包；SDK 新 ABI 的隔离安装消费仍未验证，不将宿主局部通过等同整套 SDK 交付。

**验证证据**
- F:/Projects/VisionRT/build/sdk-roi-path-recheck-test.log：test_sdk_value_contract、test_rect_interop 2/2，覆盖路径引用正反例和 Rect／Python 合同。
- 宿主 plugin_abi_contract 用真实 ABI 1 DLL 连续两次验证拒绝；红灯是 Windows 卸载后读 api 指针崩溃，修复宿主读取顺序后通过。F:/Projects/VisionRT/build/roi-abi-consumers-final-test.log 记录相关 10/10。
- F:/tmp/VisionTools.zip 已由 SDK 校验，宿主已消费该包；配置内 6 个 C++ 工具实际执行。该包仅代表当前构建选择，不定义 CPU 或其他后端包限制。

**下一步**
- 执行新 ABI 的 SDK 隔离安装消费，继续宿主绑定／路由与安装态交互验收；合同与 API 唯一入口为 docs/README.md 的源码索引。

### 2026-10-20 — 标量矩形与模块 ABI

**目标**
- 支持工具使用单个 Rect，贯通值、声明、序列化、Python 与模块边界。

**当前状态**
- 已增加 Rect 类型，复用 Roi::Rectangle；更新 Value／ValueYaml／Schema／PythonInterop，模块 ABI 主版本为 2，类型与 dtype 名称由 SDK 提供公共入口。
- 已同步到 VisionRTSDK 主仓、VisionRT 和 VisionTools 的 SDK 子模块，未提交；宿主完整安装态和旧 ABI 拒绝的新增验收仍未完成。

**验证证据**
- 宿主工作区 build/sdk-rect 中 test_sdk_value_contract、test_rect_interop 2/2 通过，含 Rect YAML、Schema 图像关联、Python float32／readonly／生命周期及非法形状拒绝。
- VisionTools 的 test_preprocess_contract、test_module_contract、test_installation_consumer 3/3 通过，消费重建的真实 DLL；日志在 F:/Projects/VisionRT/build/tools-rect-module-test.log。

**下一步**
- 隔离安装消费及旧 ABI 拒绝验收；继续宿主数值 ROI／绑定／连线集成与工具包部署，不以当前定向测试宣称整套交付。

### 2026-10-19 — 节点源端口类型引用

**目标**
- 提供单一跨流程接收声明的类型引用合同，不展开类型变体，不使用 Any。

**当前状态**
- 已完成：公共 PortTypeReference、type_from 解析、String 初始化参数引用及属性互斥校验；当前节点动态合同限于显式 host_binding，静态原生与 Python 工厂不冒充该能力。
- 三处 SDK 源码与文档同步并指向提交 `acc9b87fba27f16789c647007e6c2bc45b0c4b1b`；未推送。WORKLOG 保留本地，不进入提交。

**验证证据**
- 宿主 tools/build.cmd 顺序构建并执行 build/sdk-delivery-off、build/sdk-delivery-on 的 CTest → 各 5/5，含实际安装消费；日志 F:/Projects/VisionRT/build/receiver-sdk-*-tests.log。
- F:/tmp/VisionRT 安装版 runtime_contract → 同一个接收工具保存重开后执行 15 种完整类型，结果精确相等；宿主整体 14/14，生产者 7/7。

**下一步**
- 接口与校验以 docs/README.md 的源码和测试索引为准；宿主解析节点具体合同后再创建执行器。

### 2026-10-19 — 通用包声明与可选桥接安装消费

**目标**
- 与 VisionRT / VisionTools 使用同一包语义，验证核心 SDK 和可选 PythonInterop 的真实安装交付。

**当前状态**
- 已完成：显式 category / subcategory / resources 解析、资源存在与包内边界校验；安装声明属性不从目录名猜测。保留既有脚本逃逸检查。
- 已完成：PythonInterop ON 要求实际嵌入依赖；安装配置发现 pybind11 / Python，公共目标统一 pybind11 编译定义，避免断言与 Release 消费者对象布局分叉。
- 合同测试在 Release 中启用断言，消费语料使用当前 schema；独立安装消费者覆盖只读图像、字符串数组及解释器退出后的 C++ 所有权。
- SDK 源仓及两父工程子模块工作区源码同步；不创建 Git 提交。接口与测试索引见 `docs/README.md`。

**验证证据**
- 从 `F:/Projects/VisionRT/3rdparty/VisionRTSDK` 配置、构建并顺序执行 `ctest --test-dir build/sdk-delivery-off --output-on-failure` 与 `ctest --test-dir build/sdk-delivery-on --output-on-failure` → 各 5/5 通过，包含真实 cmake --install、find_package 和运行验证。
- ON 配置并设置 CMAKE_DISABLE_FIND_PACKAGE_pybind11=ON → 明确配置失败，不静默省略目标。
- 宿主实际 Python / NumPy 所有权、严格类型、实例刷新及安装工具执行回归通过；各父工程验证结论位于对应仓库验收报告和任务账本。
- 既有消费语料使用固定临时目录，两配置测试须顺序执行，避免跨构建进程互相清理夹具。

**下一步**
- 使用公共 CMake 目标消费 SDK；按实际 Python 与 OpenCV 环境配置，不另设转换或包校验规则。
