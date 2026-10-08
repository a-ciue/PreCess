# AGENTS.md — ZenithGrid 臻格 AI 协作开发提示词

> 本文件是面向 AI 编程助手（Codex / Copilot / Cursor 等）与人类贡献者的统一开发规范。
> 作用范围为整个仓库。提交代码前请遵循本文件，并参考 Wiki《代码提交规范》《项目文件结构》《项目贡献者指南》。
> 规则按重要程度从上到下排列：越靠前越不能违反。当有冲突时，在不确定的情况下请先询问人类贡献者。
> 当用户违反本规范、用户给出有助于项目开发的知识或是随开发过程中AGENTS.md逐渐陈旧时，AI 助手应提示用户并建议修改或补充 AGENTS.md。

---

## 0. 给 AI 助手的元指令（最高优先级）

- **先理解，再动手**：修改前先阅读相关目录的代码与依赖关系（见第 2 节），不要凭猜测改动。
- **最小化改动（默认）**：只做与当前任务直接相关的修改，保持与现有代码风格一致；不顺手重命名、不重排无关文件、不“顺便”修无关 bug（可在结论里提示）。
- **适时提出重构建议**：最小化改动不等于回避结构性问题。任务中发现现有代码存在结构性缺陷（模式误用、职责错位、重复实现、隐患级联修补等）时，先完成最小修复，同时在结论中向用户说明根因、给出重构方案与影响范围；经用户明确同意后实施的重构不受最小化改动限制，未获同意不得擅自扩大改动范围。评估优先级时以**是否引入新的复杂性维度**为重要判据：现有实现或拟议改动会引入新的复杂性维度（新概念、新状态、新依赖方向）的，优先考虑重构；不引入新维度的"整齐化"重构可暂缓。
- **根因优先**：从根本原因修复，避免表层补丁。
- **不臆造**：不确定的 API、路径、依赖必须先在仓库中检索确认；找不到就说明，不要编造。
- **尊重许可证边界**：本仓库自有代码统一采用 **LGPLv3**，第三方组件遵循各自许可证。不得引入会要求本仓库本体按 GPL / AGPL 分发的代码或依赖；引入第三方组件前须核对许可并保留原声明（见第 7 节）。
- **不擅自提交**：除非用户明确要求，不执行 `git commit` / `git push` / 建分支。
- **沟通语言**：与中文贡献者沟通、注释、commit message 默认使用中文（术语保留英文）。
- **改完即说明**：交付时简述改了什么、为什么、影响哪些模块、如何验证。

---

## 1. 项目概览

- **定位**：专注网格处理的 CAE 前处理软件，面向网格算法开发者与工业界需求。
- **架构**：**插件化架构**，功能封装在插件中，主程序运行时按需加载。
- **技术栈**：C++20、CMake、Qt Quick 6（QML）、VTK、OpenCASCADE、spdlog、Catch2。
- **目标平台**：跨平台（Windows / Linux / macOS），当前主力为 Windows + MSVC。
- **错误处理机制**：使用 **异常**（不要用错误码裸返回风格替代）。
- **迭代计划**：`README.md` 的「🗺️路线图」节是迭代计划与进度的记录，**只记计划与进行中的事项**：路线图已有条目可勾选进度或为进行中任务拆分小点；除非是计划中的大型特性，**不要为已完成的新特性向路线图新增条目**，避免路线图沦为变更日志。新完成的功能特性可考虑改以**功能特性描述**的形式补充到 README 的功能介绍处。
- **版本与接口稳定性**：项目处于**预览阶段**（0.x），接口随时可能发生二进制级/源代码级的不兼容变更，不提供稳定性承诺；项目内插件及随主程序构建的示例使用同一构建环境；SDK 独立插件须与目标主程序保持 ABI 兼容（构建与验证见第 8 节）。

---

## 2. 目录结构与依赖关系（改代码前必读）

依赖只能单向，**不要制造反向或循环依赖**：

- `core/`：项目通用基础类型，所有层都可依赖（含 `EventBus` 事件总线）。
- `model/`：业务逻辑层，依赖 `core`。
  - `model/data/`：底层数据结构（`ModelData`、`MeshData`、`ModelLayer` 等）。
  - `model/ops/`：基于数据结构的操作，依赖 `model/data`。
  - `model/systems/`：系统层（算法系统、模型 IO 系统、编辑系统、功能系统），负责插件注册与按字符串分发调用，依赖 `model/data`、`core`。
    - `model/systems/feature/`：功能系统 `FeatureSystem`，事件驱动的功能注册与调用：功能可注册参数/菜单/按键绑定，经 `EventBus` 订阅按键、参数变更、模型事件，通过 `FeatureContext` 访问模型层；声明 `interactive` 的功能另经 `InteractionContext` 订阅渲染线程驱动的视口交互（见第 10 节线程约定）。
  - `model/session/`：会话层，无 Qt 的组合根 `Session`（装配模型层 / undo 栈 / `EventBus` / 四系统并有序拆解，内部转发观察者把模型通知桥接为 `ModelEvent`）与原生查询 `SessionQuery`（返回类型化结构体），供 QML 适配层与脚本宿主共用，依赖 `model/data`、`model/ops`、`model/systems`、`core`。
- `app/`：程序与界面实现，依赖 `model`、`core`。
  - `app/core/` → `core`
  - `app/model/` → `model`、`core`、`app/core`（model 的 Qt 接口、数据绑定；可选内嵌 Python 运行时 `QPythonRuntime`，见第 10 节 Python 嵌入约定）
  - `app/render/` → `app/model`（VTK 渲染窗口控件）
  - `app/dock/`：内嵌停靠组件（`tree/` 布局树、`docking/` 停靠语义与拖放、`ui/` QtQuick 视图与 QML），对外仅暴露 `Docking.h` 的 `dock::init(QQmlEngine*)` 与 `PreCess.Docking` QML 模块（类型 `DockHost`/`DockPanel`，枚举经 `Tokens`）；只被 `app` 使用，禁止被 `core/`、`model/`、`plugins/` 依赖；**日志例外**：该目录刻意仅依赖 Qt（Core/Gui/Quick），不引入 spdlog，QML 加载类告警沿用 `qWarning`（其余 C++ 模块仍按 spdlog 规范）
  - `app/*.qml` → `app/model`、`app/render`、`app/core`（界面布局与更新，仅做轻量数据处理，不承载主业务逻辑）
- `plugins/`：项目内产品插件，依赖 `model/systems`、`model/data`、`core`，随主工程构建，不依赖 `app`。
- `examples/`：同时支持随主程序构建和 SDK 独立构建的示例，源码及测试随 SDK 分发；主工程通过 `PRECESS_BUILD_EXAMPLES` 决定是否构建。
- `python/`：precess Python 绑定模块（pyd）与内嵌解释器宿主 `python::Runtime`（LGPLv3，无 Qt；依赖 `model/session`）。
  - `plugins/algo/`：算法插件；`plugins/io/`：模型 IO 插件；`plugins/edit/`：编辑插件；`plugins/feature/`：功能插件（`FeatureHandler`，json 的 `system` 字段为 `FeatureSystem`）。

**依赖速记**：`app → model → core`；`plugins → model + core`；QML 只调依赖包功能、不写主业务逻辑。

---

## 3. 命名规范

- **C++ 类 / 结构体 / 枚举**：大驼峰 `MyClass`、`ModelActor`；继承 `QObject` 的类以大写 `Q` 开头。
- **QML 控件名 / 文件名**：大驼峰。
- **C++ 函数 / QML 成员函数**：小驼峰，优先 `动词+领域名词`（`doSomething`、`changeRenderMode`）；bool 返回值用 `isSomething` / `trySomething`；必要时追加 `forXxx` / `withXxx` / `byXxx`。若无法用“动词+名词”概括，按单一职责拆分函数。
- **C++ 信号**：小驼峰 `on+变量+Change` 或 `on+动词+名词`（`onNameChange`、`onSendData`）；**QML 信号**同样小驼峰但去掉开头 `on`。
- **C++ 局部变量**：下划线 `model_name`、`block_id`；容器用复数 `block_ids`。
- **C++ 类成员变量（非结构体）**：下划线 + 尾下划线 `data_`、`patch_ids_`。

---

## 4. 代码格式与文件编码

- **编码**：统一 **UTF-8 无签名（无 BOM）**；**行尾 CRLF**。
- **C++ 格式**：遵循根目录 `.clang-format`（BasedOnStyle: WebKit，缩进 4 空格）。VS 中 `Ctrl+K, Ctrl+D` 格式化。
- **QML 格式**：遵循 `.qmlformat.ini`（缩进 4，行尾 native）。
- 不要手动重排已格式化文件；提交前确保通过 clang-format / qmlformat。

---

## 5. 头文件与 C++ 编码细则

- **前向声明优先**：能用前向声明就用，减少 include、隔离依赖、加速并行编译。
  - 头文件中类成员/参数为 `A*`、`A&`、`shared_ptr<A>`、`weak_ptr<A>`、`vector<A*>` 等**只持有指针/引用**时，用前向声明；在对应 `.cpp` 中再 include 完整头文件。
  - **必须 include**（不可前向声明）的情况：A 是标准库类型；A 以**值类型**作成员/参数或 `vector<A>` 等需要知道大小的容器；使用 `unique_ptr<A>` 时（需在 cpp 中分离析构）。
- **覆盖虚函数**必须写 `override`。
- **include 顺序**：从小到大、从少用到多用，按需 include。
- **include 写法**：标准库与三方库用 `<>`，本项目头文件一律用 `""`。
- **头文件路径**：禁止相对路径找头文件；找不到说明 CMake 库依赖未配好，去修 CMake。
- **new 限制**：仅在使用 Qt 框架对象、OCC 智能指针时才允许 `new` 显式初始化；其余优先栈对象 / 标准库智能指针。
- 注意 `const&` 作函数参数的生命周期陷阱。
- 参考：CppCoreGuidelines、华为 C/C++ 编程规范。

---

## 6. 注释规范（Doxygen）

- 原则：简洁的行内注释 + 必要的详细说明；注释解释“为什么”，不要复述代码。
- **文件头注释**（尤其声明全局符号的头文件）用 Doxygen 块注释：`@file`、`@brief`（可空行后接详细描述）、可选 `@author`、`@date`。
- **符号注释**（类/函数/变量/属性/信号）置于声明前：函数用 `@brief`、`@tparam`、`@param`、`@return`。
- **逻辑注释**：循环/分支前说明作用与条件；长代码用空行分段，每段首行注释说明该段作用（逻辑注释不要求 Doxygen 格式）。
- 现有代码常见风格：`/** ... */` 块注释、`//! @brief`、行尾 `//> ...`，新代码与所在文件保持一致。
- 注释会经 Doxygen 生成 API 文档（见 `Doxyfile`），保持可生成。

---

## 7. 许可证边界（务必小心）

**本仓库自有代码统一采用 LGPLv3，第三方组件遵循各自许可证。**

- **自有代码与材料**：`core/`、`model/`、`app/`、`plugins/`、`python/`、`examples/`、`sdk/`、`cmake/`、`resource/`、测试、文档及根目录构建与配置文件统一采用 LGPLv3（`LGPL-3.0-only`）。
- **第三方组件**：源码、库与资源保留原版权声明和许可证，不因所在目录或本仓库许可变更而重新许可；新增组件须核对实际版本条款。
- **分发义务**：遵守 LGPLv3 的源码提供、声明保留及适用的库替换 / 重新链接要求。允许合规闭源链接，不代表可以闭源分发对 LGPL 本体的修改。
- **外部插件**：ZenithGridAddons 等独立工程保留其自身许可证；分仓不自动豁免组合分发时的 GPL / AGPL 义务。
- **新增贡献与依赖**：自有贡献须有权按 LGPLv3 许可；第三方代码保留原声明。不得引入会要求本仓库本体按 GPL / AGPL 分发的代码或依赖。
- **依赖方向**：第 2 节的单向依赖规则继续适用；统一许可证不允许 `core/` / `model/` 反向依赖 `app/` / `plugins/`。
- **正式许可文件**：以 `LICENSE.md`、`LGPLv3-LICENSE.txt` 及其引用的 `GPLv3-LICENSE.txt` 为准；`LICENSE.zh-CN.md` 仅作中文参考。

---

## 8. 构建、测试与验证

- **构建系统**：CMake 3.27+ 与 Ninja，预设见 `CMakePresets.json`（仓库模板，机器无关）与 `CMakeUserPresets.json`（本机覆盖，不入库，见 `.gitignore`）。
- **C++ 标准**：C++20（`CMAKE_CXX_STANDARD 20`，REQUIRED；source_location 写入口诊断、stop_source 取消、char8_t/u8string 迁移等已依赖，不回退）。
- **官方 Windows 打包工具链**：MSVC 14.30 x64。构建与执行 SDK 安装、CPack 前加载该版本（`vcvars64.bat -vcvars_ver=14.30`，或 `Enter-VsDevShell` 的 `-arch=x64 -host_arch=x64 -vcvars_ver=14.30`），并核对编译器版本。此项是 SDK 官方打包要求。
- **项目内插件（plugins/）**：由主工程子目录注册，直接使用 `precess_add_*_plugin`，继承目标与依赖，无需声明 `project()`、查找 SDK 或调用 `precess_plugin_install`；装配函数自动加入 AllPlugins 并安装。
- **两用示例（examples/）**：随主程序构建由 `PRECESS_BUILD_EXAMPLES=ON` 启用；同一 CMakeLists 保留 SDK 独立入口，`project()`、SDK 查找及显式安装只在 `if(NOT PRECESS_PLUGIN_IN_TREE)` 下执行。随主程序构建时与项目内动态插件共用 `${CMAKE_BINARY_DIR}/plugins` 和 AllPlugins 安装目录（macOS 共用 bundle 目录）。SDK 独立开发另见 `sdk/skills/zenithgrid-external-plugin-development/SKILL.md`。
- **常用命令（Windows，PowerShell）**：
  - 依赖路径：`CMakePresets.json` 的 `base` 预设按环境变量 `PRECESS_DEPS` 定位依赖根（PreCess-deps）；本机具体路径在 `CMakeUserPresets.json` 的 `local-base` 中覆盖（`x64-debug-local` 等预设继承仓库预设并叠加 `local-base`）。
  - 配置：`cmake --preset x64-debug`（仓库预设，需先设置 `PRECESS_DEPS`；`x64-release` / `x64-relwithdebinfo` 同理）；本机预设为 `cmake --preset x64-debug-local`
  - 构建：`cmake --build out/build/x64-debug`（对应本机预设为 `out/build/x64-debug-local`）
  - 测试：先以 `-DBUILD_TESTING=ON` 配置（默认 OFF），再 `ctest --test-dir out/build/x64-debug --output-on-failure`
  - 注意：预设文件均为标准 JSON（无 `//` 注释），命令行 `cmake --preset` 可直接使用。
  - 注意：命令行构建须先加载 MSVC 环境（`vcvars64.bat` 或 VS Developer PowerShell），否则报标准库头文件缺失（C1083 `fstream`/`array`）；Git Bash 中调用 `cmd.exe` 需防路径转换（`MSYS_NO_PATHCONV=1`，`/c` 否则被转为 `C:/`），内联引号易出错时可改写成临时 `.bat` 调用。
- 测试框架：模块单元测试用 **Catch2**（`cmake/PreCessPluginTesting.cmake` 的 `precess_add_test` / `precess_test_link_libraries`，顶层 `include(PreCessPluginTesting)` 引入），测试代码见各模块 `test/` 目录（如 `model/data/test/`、`model/systems/feature/test/`）。
- **新特性必须配套测试用例**；修 bug 时尽量补可复现的回归测试。
- 不要向无测试的模块强行塞测试框架；遵循该模块既有测试模式。
- 工具检测用 PowerShell：例如 `Get-Command makensis`（不要用 `where makensis`）。
- **插件共享头文件需全量构建**：修改被插件共享的 `core/`、`model/` 头文件（如 `InteractionState.h`、`InteractiveTypes.h`）后必须全量构建（含插件目标）再做手动验证：插件 DLL 运行时动态加载、不是 `PreCess.exe` 的链接依赖，`cmake --build --target PreCess` 不会让插件随之重建；新旧 ABI 混用会产生难以排查的内存错乱（如"测量崩溃"即此原因）。ninja 偶见头文件变更不重编（同类 ABI 混用），构建后行为异常时先 `--target clean` 全量重编再排查。

---

## 9. Git 提交规范

- **Commit 主题**第一行：`类型: 简述`，类型取 `fix/feat/refactor/docs/style/test/chore/perf/ci/build/revert` 之一。
- 空行后正文用 Markdown 详述动机与细节，多用具体类名/包名/术语。
- **一个 commit 只做一件事**；既改 A 又改 B 时拆分提交。
- 分支命名：`feature/AmazingFeature`（功能）等；通过 Fork + Pull Request 合并到主仓库。
- **PR 验收标准**：符合编码与命名规范、文件编码 UTF-8 无 BOM、完成关联 Issue 主要任务、通过现有测试、新特性带测试、能构建且主要功能可用。

---

## 10. 插件开发要点

- 功能 `Handler` 封装进插件 `PluginHandler`，由 `SystemPluginManager` 注册到对应系统。
- 每类插件须实现对应系统接口完成数据交换；算法系统目前通过模型 IO 系统以文件读写交换模型数据。
- **目标组件不依赖对象树选中态**：按组件执行的操作，框架允许时不要强制要求用户在执行功能前于对象树中选中 component，插件不得依赖该行为；对象树传入的组件身份一律不优先依赖、只视作一种提示，目标组件应优先由参数中的选择器让用户自行选择并解析（`Selection` 的全局点 id 经 `ModelLayer::pointIdMap()` 反查所属组件，面/边类局部 id 选择携带 `component_id`）。各系统落点：
  - 编辑系统：`EditHandler::execute` 接收 `ModelLayer&` 与 `fallback_component_id`（对象树当前组件，仅提示、可为 -1）；目标组件由选择器参数解析，fallback 仅在选择未携带组件身份时兜底；示例见 `plugins/edit/CreateFacePlugin/`、`plugins/edit/DeleteFacePlugin/`。
  - 算法系统：覆盖 `AlgorithmHandler::resolveComponentId` 按参数解析目标组件，不依赖对象树传入的 `fallback_component_id`；`HandlerContext::cur_component` 同样只视作提示。
  - 功能系统：`FeatureContext::activeModel` / `activeComponent` 是对象树选中态的动态查询，只作提示；优先注册 `Selector` 类型参数（`FeatureParams`）让用户显式选择目标。
- **算法导航统一由 setup 声明**：`AlgorithmHandler::setup(AlgorithmRegistrar&)` 是唯一导航注册入口，JSON 仅保留身份，不解析 `handler.navigation` 或旁置导航文件。分类、业务名称、图标、排序和分类默认参数均在 setup 收集；不声明分类时归入其他算法。共享接口变化后重建所有插件，不兼容旧算法 DLL。
- 每个插件目录含 `*.json` 描述文件（见 `plugins/*/.../*.json`）与 `CMakeLists.txt`；新增插件参照同目录既有示例结构。
- **SDK 示例与开发引导**：`examples/` 中的 ExternalPlugin、FeatureDemoPlugin、ProgressDemoPlugin、TaskDemoPlugin、ScalePreviewPlugin 只默认分发源码及测试，随 Development 组件安装；框架开发可用 `PRECESS_BUILD_EXAMPLES=ON` 构建。`sdk/skills/zenithgrid-external-plugin-development/` 随 SDK 安装为 `skills/zenithgrid-external-plugin-development/`，独立插件开发先读取该 skill 和任务相关 references。
- 功能插件（`plugins/feature/`，json 的 `system` 字段为 `FeatureSystem`）实现 `FeatureHandler` 接口：注册时 `setup(FeatureRegistrar&, FeatureContext&)` 一次（声明参数/菜单/按键绑定 + 经 `ctx.events` 订阅事件 `KeyEvent`、`ParameterChangedEvent`、`ModelEvent`），注销时 `teardown()` 一次；功能随活动操作切换被反复 进入 `activate(FeatureContext&)` / 退出 `deactivate()`（GUI 线程，所有功能可感知，由 `FeatureSystem::setFeatureActive` 驱动）；菜单触发 `execute()`。功能可修改的范围限模型层对象（经 `FeatureContext` 的 `ModelLayer` / `ComponentOperator`）与自身视口交互状态（经 `ctx.interaction`）；示例见 `examples/FeatureDemoPlugin/`，交互功能示例见 `plugins/feature/MeasurePlugin/`，插件层预览范式（`ctx.undo` 层接口）示例见 `examples/ScalePreviewPlugin/`。
  - **功能上下文固定服务**：`FeatureContext` 与 `UndoContext` 构造时绑定所属系统和功能身份；查询、任务发布与预览控制均为始终可调用的成员方法，插件不判断接口是否装配。查询结果可空，未注入 Runner 时任务返回空，无 undo 栈时预览控制空转。宿主动态 provider 仍由 `FeatureSystem` 持有，晚装配生效。`FeatureEntry` 在容器节点内直接持有固定服务并就地构造，条目不可复制／移动；容器扩容不改变上下文、参数、信息与交互状态的地址。InteractionContext 同样在构造时固定绑定 FeatureSystem 与本条目的 InteractionState；单激活按状态身份判定，宿主刷新回调仅由系统持有并支持晚装配与替换，不逐功能注入固定转发闭包。
  - 订阅 `ctx.events.subscribe<ParameterChangedEvent>` **由网关自动按所属功能过滤**，插件只处理参数下标与业务条件。全局参数观察使用已有 `EventBus`（应用层 `Session::events()`，功能层显式 `ctx.events.bus().subscribe`）；直接总线订阅不经过功能预览网关。
  - `Button` 类型参数为无值触发器：计数器载荷，功能约定忽略值、只读参数下标；点击经 `ParameterChangedEvent` 回到功能（GUI 线程）。
- **视口交互线程约定**（声明 `interactive` 的功能，改动前先读 `InteractionState.h` 注释）：交互回调（`onPick`/`onHover`）由 **渲染线程** 调用，`annotations` 为拉取契约（功能在回调中直写、渲染层拉取绘制）；**GUI 线程不得直接修改交互状态与标注**，变更经 `requestRefresh()`（纯刷新通知，重复置位自动合并）或 `deferRefresh(op)`（操作延迟到渲染线程执行后再刷新）通知，渲染线程 `InteractionService::syncPending()` 统一消费；`setActive` 启停均自动 notify，单激活约定由 FeatureSystem 装配。会话边界的现场清理走 feature 级 `deactivate()` + `deferRefresh`：框架定序保证 `deactivate()` 先于交互下线（`setActive(false)`），下线迁移（`syncState`）先消费 `deferred_op` 再 `clearSession`，清理必执行并触发重绘。
- **任务（job）线程约定**（`model/systems/job` 的 `JobRunner`：全局唯一单槽、单工作进程、无队列——占坑成功或拒绝（返回空），算法与功能共用同一个坑；执行器由无 Qt 的 `Session` 组合持有并注入算法系统与 `FeatureSystem`，Qt 宿主只提供所属线程投递和展示）：
  - **纯计算任务 `ctx.runComputeJob(name, task)`**：任务体返回 void，不持活模型句柄，不回写模型、不创建 undo 记录、不关闭预览。需要读取模型时，先在 GUI 复制输入。失败抛异常；信号或文件输出仍可。纯计算任务也持续占用模型操作直到 GUI 清理结束，忙/未注入执行器返回空。
  - **通用模型回写**：`ctx.runWritebackJob(label, capture, compute, write, masked=false)` 的 capture/write 接收 `const ModelLayer&`／`ModelLayer& + Result& + ProgressFn`，支持多目标、结构修改和插件结果安装。带 component_id 的便利重载只要求目标存在，不硬性要求 mesh。capture 须只读、输入自持，worker 不带真实模型句柄。同步计算就是 execute 中的直接调用，无公共 Inline／Worker 参数；后台发布忙／未装配 Runner 返回空，禁止以发布失败为由绕过占用同步写。
  - **正式后台 execute 接续**：输入准备成功后 Runner 接管本次捕获与已有同 owner 预览，关闭临时层；同步作用域正常退出，不把正式边界留到事件循环。成功只成一条记录，标签与 owner 沿用原 execute；计算失败／取消回滚接管预览，不增加历史。本次 execute 写过后发布仍拒绝，包括重复写旧预览已有组件；发布后 execute 抛异常取消所属新任务。事件任务只回写原身份预览，不创建正式历史。提交体开始后抛异常保留实际部分效果的可撤销记录，不承诺通用事务回滚。
  - **反馈与受控写面**：compute/write 共用 `report(double, text)`，统一进入状态栏；计算上报含取消检查，GUI 提交上报只展示、不增加取消点。QTaskStatus 在终态消费最后一次上报，成功保留文字，失败／取消覆盖阶段反馈；同步 execute 返回文字也交由状态栏显示，侧栏不保存结果窗口。组件名称／材料使用 `ComponentOperator::setName/setMaterialId`，共用写前捕获与边界通知；已有模型重命名仍无普通 undo 元数据入口，不直接改裸字段宣称支持。const ModelLayer 查询返回 const 对象，但指针成员不构成深只读沙箱；插件仍须遵守捕获只读契约。restoreModel/restoreComponent 与 gid 恢复原语供框架恢复，不作插件普通创建入口。
  - **几何责任边界**：几何输入使用、复制、共享 TShape 的不可变性与新结果构造暂由插件维护；不得原地修改 undo 前像共享的旧形状。框架不深复制或检查底层别名，只在 GUI 经受控写接口安装结果并维护身份、索引、通知与记账。ExtrudeFace 范式由插件在 GUI 捕获独立截面副本，worker 构造与检查实体，GUI 追加并发布结果；共享 Runner 提供遮罩、阶段进度、取消和正式操作记账。
  - **组件类型化回写 `ctx.runWritebackJob(label, component_id, capture, compute, write)`**：先占用模型操作，GUI capture 接收 const ComponentOperator，worker compute 接收 Input& 与 ProgressFn，GUI write 接收 ComponentOperator&、Result& 与 ProgressFn。目标不存在返回空；准备异常释放占用。无层事件与 setup/activate 无正式边界调用在捕获前拒绝；预览回写只进入发布时同身份层。已有纯计算输入可省略 capture。masked 默认 false，只控制展示；正式缩放用 true、预览 false。遮罩禁用业务区域，GUI 事件循环、进度与取消保持响应。插件不使用通用 JobResult、隐式 apply 或共享结果槽。
  - **影子组件任务 `ctx.runComponentJob(label, component_id, task)`**（undo-writing）：任务体只拿**影子目标组件**的 `ComponentOperator` 写面（单组件 scope、无 io）；提交/丢弃由框架托管——任务以 `label` 展示，正式记录沿用发布时 execute 外层边界的标签与 owner，失败/取消原子丢弃、undo 零记录。**仅从正式执行入口发布；事件不允许隐式发起正式模型任务。调用须在当前操作边界任何模型写或非空预览之前，框架检查并拒绝逆序发布**（execute/事件回调开头）：影子 copy-in 定格于调用时刻，先写真实模型再提交会丢更新。v1 拒绝几何/映射组件与层会话进行中调用（返回空）。模型 job 不允许在预览层打开时发布；运行期模型冻结由框架驱动（忙碌遮罩 + 模型通知延后、`dispatchKeyEvent` 拒绝；其他事件可处理非模型状态，模型写在底层拒绝，用户命令不排队重放），功能无需关心。
  - **心跳与取消**：计算段进度回调每次上报即心跳（取消检查点），GUI 提交段上报只展示，无上报的任务不可中断（诚实语义）；另有两个无心跳取消点——**任务体成功返回后、提交开始前**与**提交体排队期（真正执行前的最后一刻）**，取消均生效并按取消丢弃结果（提交体一旦开始则原子完成）；所有任务进度由 `QTaskStatus` 的唯一进度槽合流，GUI 每 33ms 取最新值；计算段上报仍逐次检查取消，不受界面刷新节流影响。算法与功能的启动、终态、取消及遮罩展示均来自同一个 Runner，适配器不再另存任务状态。算法侧 `HandlerContext::report_progress` 同为心跳，范式见 `examples/ProgressDemoPlugin/`；三类任务范式见 `examples/TaskDemoPlugin/`；插件层预览 + 工作线程非阻塞组合范式（每轮固定因子、GUI 只移动结果、终态 ModelEvent 回放按最新参数接续、失败路径按框架契约丢弃；未预览直接执行也走带遮罩的类型化回写）见 `examples/ScalePreviewPlugin/`。
  - **undo/redo 与任务互斥**：所有 job（含无回写自由任务）从 GUI 准备、计算到提交/丢弃及 GUI 清理结束持续占用模型操作写权。`UndoStack::undo/redo` 在占用期记日志返回 false，不取消任务、不动模型与栈；任务真实终态后可重试。取消仅请求停止，无心跳计算真正返回前不释放写权。Qt 适配层只有恢复成功才发 `applied`。
  - **功能退出与忙时管理**：当前 `Job` 携发布功能 owner，功能切出按身份取消自己的任务；无 owner 的算法任务及其他功能任务不被误取消。`setFeatureActive` 在占用期只保留最后一个有效目标并登记一次清理，A→B→C 只进入 C，A→B→A 保持 A 的会话；非法目标不覆盖有效请求、不取消任务，清理回调中再次切换也延后应用。实际退出流程统一为 deactivate → 残留预览回滚 → 会话折叠 → 交互下线；计算未退出前保留 handler 与模型占用。功能、算法、编辑与 IO 的注册、替换、注销在占用期均抛 `ModelOperationBusy`，不取消任务、不提前移除插件列表，终态后可重试；注销空闲时同步完成。会话析构先 stop/join 再退出功能与 teardown，插件不自建退出作废令牌。主动放弃（重按预览／取消按钮）仍使用 `job->cancel()`；`Job` 内部持有 `std::stop_source`，外部仅查询 `isCancellationRequested()`，取消请求不等于真实终态。
  - handler/功能的 job 在唯一工作线程**串行**执行，插件不得自建线程写模型；进度回调在计算／提交的当前线程触发，展示方经统一进度槽交给 GUI；准备、提交/丢弃、退出清理及终态通知均在宿主线程，worker 不等待 GUI。`stop` 仅在宿主线程且准备/计算/提交回调之外调用：取消、join，再复用一次收尾，不泵事件；Runner 构造即绑定 `ModelLayer&`、可空 `UndoStack*` 和有效所属线程分发器，没有无模型／后绑定模式；外借注入只验证宿主一致。原生同步宿主不创建 Runner，继续调用 `AlgorithmSystem::call`。Runner 只保留 `run` 发布入口，内部 `JobWork::inline_compute` 支持几何／映射同步回退，准备阶段只解析一次目标，回退执行复用已解析身份与同步入口的私有执行段；算法完整异步入口为已注入共享执行器的 `AlgorithmSystem::callAsync`，不逐次传执行器或任务回调，影子准备与应用原语不公开配对。
  - **会话任务收尾**：配置异步分发器的 `Session` 创建共享 Runner，终态先释放模型占用、呈现旧任务状态，再自动重放合法模型通知；展示回调异常不跳过业务重放。`QTaskStatus` 不再以 Qt 信号驱动模型收尾。默认同步会话没有 Runner，功能异步入口返回空；外借 Runner 的独立系统宿主自行装配通知重放并在系统析构前 stop。会话拆解先 stop/join 与 GUI 清理，再退出功能和断开模型记录钩子。算法与功能共用内部 `ShadowComponent` 的复制、标脏检测与 gid 回写；算法 IO 导入仍由算法系统负责。四系统 handler 的注册变更与 Algorithm／Feature 的 Runner 重装配经 ModelLayer::assertOperationIdle 统一先检查所属线程、再检查模型占用，写段授权不豁免管理变更；任务占用期禁止注册／注销／替换，Qt 插件管理桥保持原列表并提示忙碌拒绝。
  - **模型写权限**：`ModelLayer::WriteOperation` 是带身份的唯一操作占用，以 `unique_ptr` 独占持有凭证；忙时在创建凭证前拒绝，旧身份不能释放后继操作。底层 `WriteAuthority` 控制块仍共享，保留模型移动与凭证生命周期保证。真实层组件与结构写共用模型写闸，先检查 GUI 线程亲和与当前操作写段授权，再由记录器统一判定记账归属；无归属写在修改数据、gid 与缓存前抛 ModelOperationBusy 并带调用点，记录钩子只负责捕获；worker 真实层写始终抛异常，影子层明确豁免线程闸。`WritePrivilege` 仅在框架提交/同步计算/退出清理段携有效操作身份授予，功能 owner 不作写权限凭证；`writesPending` 读同一占用，`writesFrozen` 仅派生遮罩类别。通知 flush 暂停写段授权，避免监听者在记账完成后插入写；占用由 `JobRunner` 在 GUI 一次申请与释放，没有操作回调桥；写权状态仅由所属线程维护。`ModelScope` 仅管记账与通知，不授予写权限；Job 状态、错误写入、执行闭包与回调由 Runner 私有持有，插件只查询或取消。
- **功能与界面解耦（声明 / 事件 / 上下文三原则）**：通用界面（`SideBar`、菜单、渲染窗口等）禁止按插件名 / 功能名特判。
  - **静态能力走声明链**：视口交互能力（`interactive`）等一律经 json → `HandlerMetaData` → `FeatureInfo` → `QFeatureInfo` → QML 按声明渲染；新增交互功能不得改动通用界面代码。
  - **动态状态走事件回调**：交互结果、进度等经事件 / 信号传递（如功能回写参数经 `ParameterChangedEvent` → `paramValueChanged` 信号同步 QML 显示），界面不轮询插件内部状态。
  - **环境状态走上下文访问**：活动模型 / 组件、选择集经 `FeatureContext` 固定查询方法与 `App.selection` 获取，功能不反向依赖 app 层。
  - 启停类逻辑做成幂等的状态应用（以目标状态为守卫，重复触发无副作用），避免多触发源的命令式调用堆积。
- **写路径收口（写必脏 + 操作边界 flush）**：写模型数据必须经 `ComponentOperator` 语义接口（`appendPoint`/`appendFace`/`replaceMesh`/`materializeEdge` 等）或可写入口 `editableMesh(kind)`；**获取可写入口即标脏**（Topology 类立即失效邻接懒表并记入待通知集合（去重），NonTopology 仅记集合不失效懒表），**通知由操作边界 `ModelLayer::flushNotifications()` 统一发出，插件不得手调通知**（`ComponentOperator::notifyChanged` 已删除）；`component()`/`mesh()` 只读（返回 const），只读访问不标脏。结构操作（`addGeometryComponent`/`addModel`/`removeModel`/`removeComponent`）保持即时通知，不进待通知集合。
- **操作与通知边界分离**：正式 undo 边界由 `EditSystem::call`、`AlgorithmSystem::call`、`ModelIOSystem::read`、`Session` 删除入口、`FeatureSystem::invoke`（execute）和正式 job 的 GUI 提交段建立。同步子调用共用唯一正式捕获与深度，外层决定标签、owner 与预览处置；不建立子捕获栈。结构写也须有明确边界，取消边界外即时组记录的特例；初始化与退出清理由框架显式使用 `ModelScope::Kind::Cleanup`／`UndoStack::RecordingPause`，不产生历史，仍受线程与占用约束。`FeatureEventGateway`、普通 `onKeyEvent` 与生命周期回调只 flush，不创建正式记录；网关暂存器在构造时固定绑定，仅 ModelEvent 持延后回放令牌，其他事件同步经 const& 投递；事件写必须归本功能预览。注册 `KeyBinding::execute=true` 明确声明一次性快捷命令，由框架路由 `invoke`；默认按键绑定仍为预览/非模型回调。按键在原始 EventBus 发布后经 `ModelScope::Notify` 收尾，通知异常记日志、不覆盖路由业务异常。预览 job 回写只更新发布时同身份的层，不开正式记录。通知边界还包括既有 app/QML 入口；新增执行路径须明确属于正式操作、临时预览或只读路径。
- **网格数据与点 id 约定**（详见 `MeshData.h` / `ComponentData.h` 注释）：`MeshData` 自包含（坐标常驻 `vertex_positions_`，连通性数组存组件内局部点索引）；局部点索引只增不改号、不重排（`MeshAdjacency` 持久边身份与快照恢复依赖）；`Selection` / `PickInfo` 携带全局点 id（gid），写连通性前经 `ModelLayer::pointIdMap()` 换算；整网格替换经 `ComponentOperator::replaceMesh`（gid 纪律内建），运行期加点经 `ComponentOperator::appendPoint`（原子四连），其余 gid 伴生表受控点经 `ComponentData::ensurePointGlobalIds` 补缺。
- **快照原语约定**：组件级 `ComponentOperator::takeSnapshot/restoreSnapshot`、模型级 `ModelLayer::takeModelSnapshot/restoreModel/restoreComponent` 为快照/恢复统一入口；快照只装源数据与身份数据（派生缓存——邻接边表、几何子形状 type_maps——不进快照、恢复后重建；几何 gid 向量是身份数据随快照保留），恢复含 gid 对账（点/边 gid 经 `MeshIDMap::reclaim`、几何 gid 经 `GeometryRegistry::reclaim*` 均按原值拿回、组件/模型按原 id 插回）；`restoreSnapshot` 恢复后标脏（Topology），通知延迟到操作边界 flush 统一发出；undo 后选择集清空（Selection 持有的 gid/稳定 id 不作跨 undo 保证，尽管 gid 实际按原值恢复）。
- **undo/redo 系统（混合记录模式）**：`model/data/UndoStack` 实现 `UndoRecorder` 钩子挂接 `ModelLayer::setUndoRecorder`；无 Qt 的 `Session` 构造栈并注入 IO/Edit/Algo/Feature 系统，QML 经 `QModelManager.undoStack`（`QUndoStackAdaptor`）访问。
  - **默认边界自动记录**：操作边界（`beginOperation/commitOperation`，挂点同第 10 节"操作与通知边界分离"）内组件**首次标脏**经写前钩子克隆 before-image，commit 补 after-image 成一条记录；空操作丢弃，栈深上限 `kMaxDepth=32` 溢出丢最旧。简单操作零插件代码。
  - **插件预览层（savepoint）**：`ctx.undo.beginScope(label)` 开层，`revertScope` 回起点并保持层，`cancelScope` 回滚关闭。插件控制两次 execute 之间的预览更新，层可跨事件存活，不形成快照链或正式历史项。插件面不暴露 `commitScope`/`inOperation` 配对责任。execute 收尾由框架吸收临时捕获、关闭层、补 after-image 并提交正式操作；正常与异常收尾都保留部分效果的可撤销语义。无模型改动则丢弃空记录。
  - **预览归属与收尾**：捕获覆盖层内全部组件和结构修改。事件只写自己的层，不借用其他层或外层正式边界；无层写与结构写均在动数据前拒绝，事件不能入栈、清 redo、折叠会话或确认层。唯一预览捕获独立于正式边界栈；相同 owner 的 execute 继续预览捕获，写保留最早 before-image；执行结束后关闭层，含 execute 内新开层及已有非空预览但 execute 无新增写的情况。其他正式入口在解析目标或准备影子快照之前先回滚旧预览；execute 内同步嵌套调用继续外层捕获；只读事件不抢占。预览回写带发布时层身份，迟到结果不能落入重开的层。
  - **功能会话（activate → deactivate 收尾折叠成一条）**：`setFeatureActive` 进入时 `beginSession(功能唯一名, 功能显示名)`、退出时 `endSession`。会话**不是帧**——不占帧栈，因此不锁撤销、模态事件循环规则与层的归属判定都不受影响（会话跨任意多次事件轮次是设计使然）；只给**会话所有者**成的记录（execute / 正式任务回写，经 `OwnerScope` 打上所有者）打会话标，收尾把**栈顶连续**的同标记录折叠成一条（一个功能会话 = 一条撤销条目），夹在中间的旁路记录不折叠——跨过别人的记录合并会把别人的改动卷进同一条前像，撤销时会多撤。会话期内每步仍是独立记录、可逐步撤销；**撤销只走统一入口**（界面/`undo()`），会话期与会话外都撤销栈顶记录并移入 redo，不另设会话撤销分支——**插件面不暴露任何撤销 API**（`UndoContext` 只有临时层控制接口）。
  - **模态嵌套事件循环禁令**：操作边界（`beginOperation/commitOperation`）打开期间与任务回写提交段执行期间，禁止启动嵌套事件循环（模态对话框、`QEventLoop::exec`、任何本地事件泵）。边界是同步段——一旦跨过事件循环轮次，插队事件就可能落进未收尾的边界，造成记录错乱或回写中断。三道运行时守卫（撤销入口在边界内拒绝、恢复中拒绝重入开记录、异步提交遇未闭合边界时一次拒绝并按 Failed 收尾，不重投）是兜底不是许可：异步提交撞上未闭合边界时优先检查嵌套事件循环；恢复中的观察者同步回调也不得开正式操作。
  - **预览执行路径规则**：层打开时用户 undo=`cancelScope`（回滚关闭，不动历史），redo 空转；任务占用期 undo/redo 仍拒绝。插件开新层丢弃旧层，同 owner execute 使用预览并在收尾确认；其他正式入口抢占时回滚关闭。功能退出残留层由框架取消，忙时先取消任务并延后清理。导出为所见即所得，包含临时预览态。
  - **undo 后选择集清空已机制化**：`QUndoStackAdaptor::applied` 信号 → QML 统一 `clearSelection`（CentralRenderArea）。
  - **结构操作**：正式边界内并入当前操作，预览内归临时捕获并随层回滚或 execute 收尾确认；事件中的结构写也必须有本功能预览，禁止在事件里即时成记录。IO 导入与 Session 删除由框架建立明确边界；有记录器的裸结构写在动数据前拒绝，初始化／退出清理显式抑制记录。
- **Python 嵌入约定**（宿主为 `python/` 的 `python::Runtime` 无 Qt 静态库，GUI 的 `QPythonRuntime` 仅 QObject/QML 薄壳、无头 CLI 将复用同一宿主；`precess_runtime` 目标恒存在，Python3 + pybind11 缺失、`PRECESS_BUILD_PYTHON=OFF` 或 wasm 时自动降级为桩实现——"不可用"是运行时状态（available 恒 false），宿主侧不写条件编译）：**Python 与 GUI 线程绑定**，所有 Python 入口须在 GUI 线程调用，渲染线程不得触碰 Python；解释器懒初始化（控制台首次使用时启动），标准库根（python_home）由宿主运行期探测：随包分发的可移植标准库 `<exe_dir>/Lib` → 构建期导出的 `PRECESS_PYTHON_HOME_DIR`（存在才用）；安装规则把 pyd 装到 `<exe_dir>/python`、`Lib/`/`DLLs/` 随包分发到 exe 目录，构建树由 `precess_bindings` 构建后把解释器运行库（Windows 的 python3xx.dll，经 FindPython 导入目标求值、不硬编码版本）拷至构建根目录——Qt 工程运行时产物统一在此，运行/调试不依赖 PATH（类 Unix 的 libpython 由 rpath 解析，无需拷贝），解释器选择经可选缓存变量 `PRECESS_PYTHON_PATH`（解释器所在目录，经 `Python3_ROOT_DIR` 转交 FindPython、平台命名差异由模块处理，仅构建期生效，留空用系统 PATH 发现），`sys.path` 注入 precess 扩展模块目录（候选 `<exe_dir>/python` 与构建树输出目录）；**活会话以引用策略注入 `precess.current`**（Python 不持有所有权），故宿主析构必须先终结解释器、再析构会话；同时含 Qt 头与 Python/pybind11 头的编译单元，须取消并随后还原 Qt 的 `slots/signals/emit` 宏（否则破坏 CPython `PyType_Spec::slots`）；precess pyd 与插件同理，须与主程序同编译器、同配置、同依赖版本构建。**延时回调走 app 侧 pybind11 注册专属模块**：app/model 的 `QPythonAppModule` 经 pybind11 直接创建 app 专属可 import 子模块 `precess.app`（挂父模块属性 + 登记 sys.modules，`precess` 是单 pyd 扩展模块无 `__path__`，导入器发现不了其下子模块）并注册任意签名本机函数（pybind11 全套类型转换器可用；后续 app 侧 Python 函数统一收在该子模块，对模块句柄继续 `def` 即加）；pybind11 头仅真实模式可用，真/桩编入恒存在的 `pythonApp` 静态库目标（按 `precess_bindings` 存在性在目标定义处一次判定，同 `precess_runtime` 模式；`QPythonRuntime` 接入层亦编入其中，QML 类型经 modelQml 的 `QPythonRuntimeQml.h` 外来包装注册），消费方（modelQml、测试）直接链接，业务代码无 `#ifdef`；宿主配置宏（`PRECESS_PYTHON_HOME`/`PRECESS_PYTHON_MODULE_DIR`）随 `precess_runtime` PUBLIC 传递；定时策略（QTimer 单发定时器表）、回调唤起与生命周期全由 app 管理（约定 GUI 主线程，回调异常格式化记日志），关停须先于解释器终结（`py::object` 先释放）；裸 precess 绑定不含 app 函数（`precess.app` 由宿主注册，契约由 test_precess.py 固定）。

---

## 11. 提交前自检清单（AI 与人类通用）

- [ ] 改动范围最小、与任务直接相关，未引入无关变更。
- [ ] 命名、注释、格式符合第 3–6 节。
- [ ] 文件 UTF-8 无 BOM、CRLF 行尾。
- [ ] 依赖方向正确，无循环/反向依赖；自有贡献遵循 LGPLv3，第三方声明完整，新增依赖不改变本体的 LGPLv3 许可模式。
- [ ] 头文件按需前向声明 / include，无相对路径 include。
- [ ] 通用界面无插件名 / 功能名特判；插件静态能力、动态状态、环境状态分别经声明链、事件回调、上下文访问（第 10 节三原则）。
- [ ] 视口交互状态仅由渲染线程修改；GUI 线程变更经 `requestRefresh` / `deferRefresh` 通知（第 10 节线程约定）。
- [ ] 能通过构建；涉及逻辑改动已（或建议）跑测试，新特性带测试。
- [ ] 路线图同步合规：仅为路线图中已有（计划/进行中）条目勾选进度或拆分小点；未为已完成的新特性新增路线图条目（新功能按需补充功能特性描述，见第 1 节）。
- [ ] Commit 类型正确、单一职责、信息清晰。
