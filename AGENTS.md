# AGENTS.md — PreCess 前蔚处理 AI 协作开发提示词

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
- **尊重许可证边界**：`core/`、`model/`、`cmake/` 使用 **LGPLv3**（保留闭源二次开发能力），`app/`、`plugins/`、`resource/` 及其余目录使用 **AGPLv3**；不要把 AGPL/GPL 代码引入 LGPL 目录，新增依赖前必须确认许可证与所在目录兼容（见第 7 节）。
- **不擅自提交**：除非用户明确要求，不执行 `git commit` / `git push` / 建分支。
- **沟通语言**：与中文贡献者沟通、注释、commit message 默认使用中文（术语保留英文）。
- **改完即说明**：交付时简述改了什么、为什么、影响哪些模块、如何验证。

---

## 1. 项目概览

- **定位**：专注网格处理的 CAE 前处理软件，面向网格算法开发者与工业界需求。
- **架构**：**插件化架构**，功能封装在插件中，主程序运行时按需加载。
- **技术栈**：C++17、CMake、Qt Quick 6（QML）、VTK、OpenCASCADE、spdlog、GoogleTest。
- **目标平台**：跨平台（Windows / Linux / macOS），当前主力为 Windows + MSVC。
- **错误处理机制**：使用 **异常**（不要用错误码裸返回风格替代）。

---

## 2. 目录结构与依赖关系（改代码前必读）

依赖只能单向，**不要制造反向或循环依赖**：

- `core/`：项目通用基础类型，所有层都可依赖（含 `EventBus` 事件总线）。
- `model/`：业务逻辑层，依赖 `core`。
  - `model/data/`：底层数据结构（`ModelData`、`MeshData`、`ModelLayer` 等）。
  - `model/ops/`：基于数据结构的操作，依赖 `model/data`。
  - `model/systems/`：系统层（算法系统、模型 IO 系统、编辑系统、功能系统），负责插件注册与按字符串分发调用，依赖 `model/data`、`core`。
    - `model/systems/feature/`：功能系统 `FeatureSystem`，事件驱动的功能注册与调用：功能可注册参数/菜单/按键绑定，经 `EventBus` 订阅按键、参数变更、模型事件，通过 `FeatureContext` 访问模型层；声明 `interactive` 的功能另经 `InteractionContext` 订阅渲染线程驱动的视口交互（见第 10 节线程约定）。
- `app/`：程序与界面实现，依赖 `model`、`core`。
  - `app/core/` → `core`
  - `app/model/` → `model`、`core`、`app/core`（model 的 Qt 接口、数据绑定）
  - `app/render/` → `app/model`（VTK 渲染窗口控件）
  - `app/*.qml` → `app/model`、`app/render`、`app/core`（界面布局与更新，仅做轻量数据处理，不承载主业务逻辑）
- `plugins/`：插件示例与二次开发，依赖 `model/systems`、`model/data`、`core`，与 `app` 独立构建。
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

本仓库采用**双许可证**结构，按目录区分：

- **`core/`、`model/`、`cmake/`：LGPLv3**
  - `core/` / `model/` 作为独立库使用时按 LGPLv3 处理，允许在不修改依赖源码前提下被闭源软件链接。
  - `cmake/` 仅在构建期运行，不会链接进最终二进制；采用 LGPLv3 便于下游 LGPL 使用者直接复用其中的 Find 模块。
  - **禁止**在这些目录中引入 GPL / AGPL / 或与 LGPLv3 不兼容的代码；否则会破坏"闭源二次开发"能力。新增头文件、依赖时必须核对许可证。
- **`app/`、`plugins/`、`resource/` 及其余目录：AGPLv3**
  - 之所以采用 AGPLv3，是因为插件依赖链中可能包含以 AGPLv3 发布的第三方库；AGPLv3 会"传染"到全部链接使用它的代码。
  - `resource/` 属于应用品牌与 Windows 资源，随 `app/` 一同分发，因此归入 AGPLv3。
  - 修改或分发这部分代码必须遵守 AGPLv3，包括第 13 条"通过网络提供服务时必须向使用者提供对应源代码"。
  - 新增依赖前必须确认其许可证与 AGPLv3 兼容；**禁止**引入与 AGPLv3 不兼容的许可证代码。

**依赖方向与许可证的一致性**：

- `app/` / `plugins/`（AGPLv3）可以正常链接 `core/` / `model/`（LGPLv3），LGPLv3 与 AGPLv3 兼容。
- **反向禁止**：`core/` / `model/` 不允许出现对 `app/` / `plugins/` 的代码依赖或包含 AGPL 头文件；这既违反第 2 节的依赖方向，也会把 AGPL 传染回核心库。
- 单独发行仅使用 `core/` + `model/` 的衍生作品（不打包任何 AGPL 组件）可继续按 LGPLv3 分发。
- 一旦发行版打包了任何 AGPLv3 组件，整体分发义务按 AGPLv3 处理。

---

## 8. 构建、测试与验证

- **构建系统**：CMake + Ninja，预设见 `CMakePresets.json` / `CMakeUserPresets.json`。
- **C++ 标准**：C++17（`CMAKE_CXX_STANDARD 17`，REQUIRED）。
- **常用命令（Windows，PowerShell）**：
  - 配置：`cmake --preset x64-debug`（或 `x64-release` / `x64-relwithdebinfo`）
  - 构建：`cmake --build out/build/x64-debug`
  - 测试：先以 `-DBUILD_TESTING=ON` 配置（默认 OFF），再 `ctest --test-dir out/build/x64-debug --output-on-failure`
  - 注意：`CMakeUserPresets.json` 含 `//` 注释，VS 的 CMake 集成可以容忍，但命令行 `cmake --preset` 会因解析失败而报错；命令行场景请手动传参（参照 preset 中的变量）或直接复用已配置好的构建目录。
  - 注意：命令行构建须先加载 MSVC 环境（`vcvars64.bat` 或 VS Developer PowerShell），否则报标准库头文件缺失（C1083 `fstream`/`array`）；Git Bash 中调用 `cmd.exe` 需防路径转换（`MSYS_NO_PATHCONV=1`，`/c` 否则被转为 `C:/`），内联引号易出错时可改写成临时 `.bat` 调用。
- 测试框架：模块单元测试用 **Catch2**（`cmake/test.cmake` 的 `precess_add_test` / `precess_test_link_libraries`），测试代码见各模块 `test/` 目录（如 `model/data/test/`、`model/systems/feature/test/`）。
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
- 每个插件目录含 `*.json` 描述文件（见 `plugins/*/.../*.json`）与 `CMakeLists.txt`；新增插件参照同目录既有示例结构。
- 功能插件（`plugins/feature/`，json 的 `system` 字段为 `FeatureSystem`）实现 `FeatureHandler` 接口：注册时 `setup(FeatureRegistrar&, FeatureContext&)` 一次（声明参数/菜单/按键绑定 + 经 `ctx.events` 订阅事件 `KeyEvent`、`ParameterChangedEvent`、`ModelEvent`），注销时 `teardown()` 一次；功能随活动操作切换被反复 进入 `activate(FeatureContext&)` / 退出 `deactivate()`（GUI 线程，所有功能可感知，由 `FeatureSystem::setFeatureActive` 驱动）；菜单触发 `execute()`。功能可修改的范围限模型层对象（经 `FeatureContext` 的 `ModelLayer` / `ComponentOperator`）与自身视口交互状态（经 `ctx.interaction`）；示例见 `plugins/feature/FeatureDemoPlugin/`，交互功能示例见 `plugins/feature/MeasurePlugin/`，staged 预览范式（`"undo": "manual"` + `ctx.undo` staged 会话）示例见 `plugins/feature/ScalePreviewPlugin/`。
  - 订阅 `ParameterChangedEvent` **必须按 `e.feature` 过滤**本功能注册名（与 json 一致，参照 FeatureDemoPlugin 的 `kFeatureName` 常量），否则将响应其他功能的参数变更。
  - `Button` 类型参数为无值触发器：计数器载荷，功能约定忽略值、只读参数下标；点击经 `ParameterChangedEvent` 回到功能（GUI 线程）。
- **视口交互线程约定**（声明 `interactive` 的功能，改动前先读 `InteractionState.h` 注释）：交互回调（`onPick`/`onHover`）由 **渲染线程** 调用，`annotations` 为拉取契约（功能在回调中直写、渲染层拉取绘制）；**GUI 线程不得直接修改交互状态与标注**，变更经 `requestRefresh()`（纯刷新通知，重复置位自动合并）或 `deferRefresh(op)`（操作延迟到渲染线程执行后再刷新）通知，渲染线程 `InteractionService::syncPending()` 统一消费；`setActive` 启停均自动 notify，单激活约定由 FeatureSystem 装配。会话边界的现场清理走 feature 级 `deactivate()` + `deferRefresh`：框架定序保证 `deactivate()` 先于交互下线（`setActive(false)`），下线迁移（`syncState`）先消费 `deferred_op` 再 `clearSession`，清理必执行并触发重绘。
- **功能与界面解耦（声明 / 事件 / 上下文三原则）**：通用界面（`SideBar`、菜单、渲染窗口等）禁止按插件名 / 功能名特判。
  - **静态能力走声明链**：视口交互能力（`interactive`）等一律经 json → `HandlerMetaData` → `FeatureInfo` → `QFeatureInfo` → QML 按声明渲染；新增交互功能不得改动通用界面代码。
  - **动态状态走事件回调**：交互结果、进度等经事件 / 信号传递（如功能回写参数经 `ParameterChangedEvent` → `paramValueChanged` 信号同步 QML 显示），界面不轮询插件内部状态。
  - **环境状态走上下文访问**：活动模型 / 组件、选择集经 `FeatureContext` provider 与 `App.selection` 获取，功能不反向依赖 app 层。
  - 启停类逻辑做成幂等的状态应用（以目标状态为守卫，重复触发无副作用），避免多触发源的命令式调用堆积。
- **写路径收口（写必脏 + 操作边界 flush）**：写模型数据必须经 `ComponentOperator` 语义接口（`appendPoint`/`appendFace`/`replaceMesh`/`materializeEdge` 等）或可写入口 `editableMesh(kind)`；**获取可写入口即标脏**（Topology 类立即失效邻接懒表并记入待通知集合（去重），NonTopology 仅记集合不失效懒表），**通知由操作边界 `ModelLayer::flushNotifications()` 统一发出，插件不得手调通知**（`ComponentOperator::notifyChanged` 已删除）；`component()`/`mesh()` 只读（返回 const），只读访问不标脏。结构操作（`addGeometryComponent`/`addModel`/`removeModel`/`removeComponent`）保持即时通知，不进待通知集合。
- **操作边界清单**：`EditSystem::call`、`AlgorithmSystem::call`、`FeatureSystem::invoke`（含 `dispatchKeyEvent` 按键路由）、`FeatureSystem` 生命周期回调（`activate`/`deactivate`/`teardown`，功能退出清理现场等模型写经此 flush；只 flush 不成 undo 记录——退出清理若可撤销，撤销后会留下功能已停止跟踪的游离状态）、`FeatureEventGateway` 包装的事件回调（功能经 `ctx.events` 订阅的回调返回后自动 flush，异常时先 flush 再重抛）、app 层 QML 入口（`QModelManager::removeMesh/removeGeometry`、`QGeometryOperations::addGeometryShape` 组件分支）；Edit/Algo/QML 入口的 flush 为过渡 shim（随系统迁移消亡），FeatureSystem 的 invoke/生命周期回调 flush 与 `FeatureEventGateway` 为长期设施。**新增插件代码执行路径须纳入边界**，否则标脏的通知不会发出。
- **网格数据与点 id 约定**（详见 `MeshData.h` / `ComponentData.h` 注释）：`MeshData` 自包含（坐标常驻 `vertex_positions_`，连通性数组存组件内局部点索引）；局部点索引只增不改号、不重排（`MeshAdjacency` 持久边身份与快照恢复依赖）；`Selection` / `PickInfo` 携带全局点 id（gid），写连通性前经 `ModelLayer::pointIdMap()` 换算；整网格替换经 `ComponentOperator::replaceMesh`（gid 纪律内建），运行期加点经 `ComponentOperator::appendPoint`（原子四连），其余 gid 伴生表受控点经 `ComponentData::ensurePointGlobalIds` 补缺。
- **快照原语约定**：组件级 `ComponentOperator::takeSnapshot/restoreSnapshot`、模型级 `ModelLayer::takeModelSnapshot/restoreModel/restoreComponent` 为快照/恢复统一入口；快照只装源数据与身份数据（派生缓存——邻接边表、几何子形状 type_maps——不进快照、恢复后重建；几何 gid 向量是身份数据随快照保留），恢复含 gid 对账（点/边 gid 经 `MeshIDMap::reclaim`、几何 gid 经 `GeometryRegistry::reclaim*` 均按原值拿回、组件/模型按原 id 插回）；`restoreSnapshot` 恢复后标脏（Topology），通知延迟到操作边界 flush 统一发出；undo 后选择集清空（Selection 持有的 gid/稳定 id 不作跨 undo 保证，尽管 gid 实际按原值恢复）。
- **undo/redo 系统（混合记录模式）**：`model/data/UndoStack` 实现 `UndoRecorder` 钩子挂接 `ModelLayer::setUndoRecorder`；app 层 `QModelManager` 构造栈并注入 Edit/Algo/Feature 系统，QML 经 `QModelManager.undoStack`（`QUndoStackAdaptor`）访问。
  - **默认边界自动记录**：操作边界（`beginOperation/commitOperation`，挂点同第 10 节"操作边界清单"）内组件**首次标脏**经写前钩子克隆 before-image，commit 补 after-image 成一条记录；空操作丢弃，栈深上限 `kMaxDepth=32` 溢出丢最旧。简单操作零插件代码。
  - **Manual 插件自控**：功能 json 声明 `"undo": "manual"`（→ `HandlerMetaData::undo_manual`）后，`invoke`/按键路由/事件网关边界不再自动捕获/提交（只 flush），插件经 `ctx.undo`（`UndoContext`）的 **staged 会话**显式控制，before-image 由栈持有（非插件自持）。
  - **staged 会话（v1 单组件、无快照链）**：`beginStaged`（栈捕获 before₀）→ `editableMesh` 预览写，重试经 `revertStaged` 回滚再改 → 确认 `commitStaged`（before₀+当前状态成一条记录）/ 取消 `cancelStaged`（恢复 before₀ 不成记录）。逐步回退需求由 Auto 模式"每次执行一条记录"覆盖。
  - **执行路径规则**：staged 打开时 undo=`cancelStaged`（恢复 before₀ 并关闭会话，不动全局栈）、redo 空转；隐式 `cancelStaged` 兜底挂在**真实写入点**（边界内首次标脏、结构操作）而非 `beginOperation`——纯旁观回调（只读事件订阅同样走操作边界）不得误杀进行中的预览，旧功能后续 staged 调用空转容忍；导出为只读所见即所得（含预览态），`stagedActive` 已暴露 QML 供界面禁用入口；功能 `deactivate` 时须自行关闭 staged 会话。
  - **undo 后选择集清空已机制化**：`QUndoStackAdaptor::applied` 信号 → QML 统一 `clearSelection`（CentralRenderArea）。
  - **结构操作即时成记录**：`addModel`/`removeModel`/`removeComponent`/`addGeometryComponent` 由钩子即时成记录；边界内发生的结构操作并入当前操作（一次用户动作一条记录）。

---

## 11. 提交前自检清单（AI 与人类通用）

- [ ] 改动范围最小、与任务直接相关，未引入无关变更。
- [ ] 命名、注释、格式符合第 3–6 节。
- [ ] 文件 UTF-8 无 BOM、CRLF 行尾。
- [ ] 依赖方向正确，无循环/反向依赖；`core/` / `model/` / `cmake/` 未引入 AGPL/GPL 代码，`app/` / `plugins/` / `resource/` 新增依赖与 AGPLv3 兼容。
- [ ] 头文件按需前向声明 / include，无相对路径 include。
- [ ] 通用界面无插件名 / 功能名特判；插件静态能力、动态状态、环境状态分别经声明链、事件回调、上下文访问（第 10 节三原则）。
- [ ] 视口交互状态仅由渲染线程修改；GUI 线程变更经 `requestRefresh` / `deferRefresh` 通知（第 10 节线程约定）。
- [ ] 能通过构建；涉及逻辑改动已（或建议）跑测试，新特性带测试。
- [ ] Commit 类型正确、单一职责、信息清晰。