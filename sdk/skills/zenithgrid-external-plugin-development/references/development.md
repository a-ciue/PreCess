# 插件 API 与生命周期

以所用 SDK 的头文件为准：`FeatureHandler.h`、`FeatureRegistrar.h`、`FeatureNavigation.h`、`FeatureContext.h`、`ComponentOperator.h` 和交互相关头文件。SDK 处于预览阶段，接口可能不兼容。

## 注册、参数和导航

- 插件继承 `QObject` 和 `systems::PluginBase`，通过 `HandlerCreatorDestroyerFactory` 在插件侧创建、销毁 Handler，保持 DLL 边界两侧分配释放配对。
- 新增算法、编辑和业务功能统一使用 `FeatureHandler`，JSON 的 `system` 为 `FeatureSystem`；IO 使用 `ModelIOHandler` / `ModelIOSystem`。旧 Algorithm / Edit 接口仅兼容已有调用者，已废弃并冻结。
- `setup` 声明参数、菜单、按键和事件订阅；`activate/deactivate` 对应反复进出功能，`teardown` 对应注销。上下文服务始终可调用，查询或任务结果可空；订阅句柄须保活并随生命周期退订。
- `ctx.events.subscribe<ParameterChangedEvent>` 自动按所属功能过滤，全局监听用 `ctx.events.bus()`；Button 参数只处理参数下标。目标优先用 Selector，活动组件只作提示；点 gid 经 `ModelLayer::pointIdMap()` 解析组件，局部边/面选择携 `component_id`。

## 统一功能入口

功能入口通过 `navigation().addEntry` 注册：

```cpp
registrar.addParameter({ ArgTypeEnum::Int, "迭代数", "100" });
registrar.navigation().addEntry({ "analysis", "分析", "", 20, "功能/求解" });
registrar.navigation().setLabel("稳态求解器");
```

`FeatureNavigationEntry` 依次为稳定 `id`、入口标题、入口图标、入口排序、`menu_path`。不同功能注册相同 id 即属于同一个入口；一个子功能直接显示参数，多个子功能显示选择器。同一功能可声明多个入口，共享一份参数和执行状态。id 不从标题或图标推断；同一功能内重复 id 保留首个有效声明，空 id 或标题被忽略。

`menu_path` 使用“菜单”或“菜单/分组”，留空时继承宿主同 id 的路径，最终回退“功能”。分组只由该路径表达。`setLabel/setIcon/setOrder` 配置子功能的显示名称、图标与排序，不覆盖入口描述；`setEntryDefault(id, parameter, value)` 声明入口参数预设（Combo 为选项索引字符串）。宿主可通过 `FeatureSystem::setNavigationEntries` 保留常驻入口；零子功能显示空态并禁用执行。未声明入口的功能自动归入默认“功能”菜单。

导航在 `setup` 声明，JSON 保留身份与能力。

## 模型写与 undo

- 写入使用 `ComponentOperator` 的语义函数或 `editableMesh(MeshEditKind)`；申请可写网格即标脏，拓扑修改用 `Topology`，仅坐标变化用 `NonTopology`。通知由框架收尾，不直接改裸字段或手调通知。
- 正式 `execute` 由框架记 undo；事件只写自己的预览层，通过 `ctx.undo.beginScope/revertScope/cancelScope` 管理，正式执行确认、退出回滚由框架负责。插件不调用 undo/redo，操作边界内禁止模态对话框或嵌套事件循环；契约见 `FeatureContext.h`。
- 真实模型属于宿主线程；后台输入须只读捕获并自持，const 指针成员不保证深只读。几何结果由插件构造，不原地修改与 undo 前像共享的 TShape。

## 后台任务

算法与功能共享单槽 `JobRunner`，无队列；发布被拒绝返回空，不得同步写模型绕过占用，也不得自建线程写真实模型。

- `runComputeJob`：纯计算或外部输出，不回写模型、不创建 undo、不关闭预览；模型输入先在宿主线程复制。
- `runWritebackJob`：宿主线程 capture 只读捕获 → worker compute → 宿主线程 write 受控提交，支持组件或模型层重载。正式任务接续原执行/预览，事件任务只回写原身份预览；提交异常可能留下可撤销的部分结果，不保证通用事务回滚。
- `runComponentJob`：仅正式执行、任何模型写和非空预览之前发布，worker 只写影子组件；失败/取消丢弃影子。当前拒绝几何/映射组件与层会话；签名见 `FeatureContext.h`，范式见 `examples/TaskDemoPlugin`、`examples/ScalePreviewPlugin`。

所有任务都占用模型写权至真实终态，其间拒绝 undo/redo；`masked` 只控制遮罩。计算段 `report` 是进度心跳和取消检查，提交段只展示；提交开始前仍可取消，开始后完成提交。无心跳计算无法及时取消，`job->cancel()` 仅请求停止；退出取消、等待和清理由框架负责。

## 视口交互与示例

`onPick/onHover` 在渲染线程运行，可写交互状态和标注；GUI 使用 `requestRefresh()` 或 `deferRefresh(op)`，不直接修改。能力、结果和环境分别走声明、事件、上下文，不依赖 app 层；契约见 `InteractionState.h`、`InteractionContext.h`，生命周期与事件示例见 `examples/FeatureDemoPlugin`。

公共头文件变化后，使用匹配 SDK、工具链和依赖全量重建主程序及全部插件，确保 ABI 一致。构建、安装与加载验证见 [build.md](build.md)。
