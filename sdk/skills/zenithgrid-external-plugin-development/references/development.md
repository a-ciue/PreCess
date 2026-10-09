# 插件 API 与生命周期

以所导入 SDK 的头文件为最终依据：`include/precess/model/systems/{core,feature,algo,edit,io,job}`、`model/ops`、`model/data` 和 `model/session`。SDK 处于预览阶段，API 可发生不兼容变化，不把旧示例签名当作稳定承诺。

## 注册、参数和目标

- 插件继承 QObject 和 systems::PluginBase，用 HandlerCreatorDestroyerFactory 由插件创建/销毁 Handler，保持 DLL 边界两侧分配释放配对。新增算法、编辑和通用业务功能使用 FeatureHandler；IO 使用 ModelIOHandler。AlgorithmHandler / AlgorithmSystem 与 EditHandler / EditSystem 已废弃并冻结，仅维护已有调用者，不新增旧插件或扩展旧接口；先查头文件确认当前纯虚接口。
- 新增业务插件 JSON 的 system 为 FeatureSystem，IO 为 ModelIOSystem；AlgorithmSystem、EditSystem 仅用于已有插件兼容，handler.name 唯一。视口交互能力通过 JSON 的 interactive 声明，宿主读取声明，不能要求通用界面按插件名特判。
- 功能 setup(FeatureRegistrar&, FeatureContext&) 注册参数、菜单、按键和订阅；activate/deactivate 对应重复进出功能；teardown(FeatureContext&) 对应注销。上下文固定绑定系统与 owner，服务始终可调用；查询或任务结果可能为空，不检测接口是否装配。
- `ctx.events.subscribe<ParameterChangedEvent>` 自动按所属功能过滤；全局监听明确使用 ctx.events.bus()。订阅句柄须保活并随生命周期退订。Button 参数只按 param_index 处理，计数载荷不作为值。
- 优先注册 Selector 让用户选择目标；活动对象树组件只作 fallback。全局点 gid 经 ModelLayer::pointIdMap() 解析组件，局部边/面选择携 component_id。旧算法兼容实现覆盖 resolveComponentId，旧编辑 execute 接收 ModelLayer 与 fallback_component_id；新增业务通过 Feature Selector 解析目标。不要把例子的活动组件简化带入正式功能。

## 模型写与 undo

写入走 ComponentOperator 的语义函数（appendPoint、appendFace、replaceMesh、setName、setMaterialId 等）或 editableMesh(MeshEditKind)。component()/mesh() 返回 const；申请可写网格即标脏，Topology 使邻接缓存失效，NonTopology 用于仅坐标变化。通知由框架操作边界 flush，插件不手调通知或修改裸数据字段绕过记录器。

正式 execute / 系统 call 由框架建立 undo 边界。事件只能写自己的预览层：ctx.undo.beginScope、revertScope、cancelScope、scopeActive 等以 UndoContext.h 为准；事件不能创建正式历史或确认他人预览。正式 execute 接续同 owner 预览并由框架收尾确认，切换功能回滚残留预览。插件没有 undo/redo 公共控制入口。边界中禁止模态对话框、QEventLoop::exec 或事件泵。

真实模型属于宿主线程；后台任务不能持真实 ModelLayer / ComponentOperator 可写句柄。const 查询并不保证指针成员深只读；capture 必须自觉只读并复制自持输入。几何共享 TShape 不可原地改动 undo 前像，独立复制与新结果构造由插件负责。

## 后台任务

算法与功能共享唯一 JobRunner，单槽无队列。任务发布忙碌或未注入 Runner 返回空，不能因此同步写模型绕过占用。默认同步 Session 无 Runner；异步宿主使用 Session 配置分发器，由框架 stop/join 和终态清理。不要自建线程写模型。

- `ctx.runComputeJob(name, task)`：task(ProgressFn) 返回 void，纯计算或外部输出，不回写模型、不创建 undo、不关闭预览。需要模型输入时先在 GUI 复制。
- `ctx.runWritebackJob(label, component_id, capture, compute, write, masked=false)`：GUI capture(const ComponentOperator&) 只读捕获；worker compute(Input&, ProgressFn) 返回结果；GUI write(ComponentOperator&, Result&, ProgressFn) 受控提交。也有 ModelLayer 通用重载，支持多目标和结构结果；查 FeatureContext.h 确认重载。准备成功后框架接管原 execute / 预览，正式成功仅形成一条记录；事件任务仅回写发布时同身份预览。提交中抛异常可能保留部分效果的可撤销记录，不能假设通用事务回滚。
- `ctx.runComponentJob(label, component_id, task)`：worker 只拿影子组件；仅正式 execute、任何模型写和非空预览之前发布。失败/取消丢弃影子，不增加历史；v1 拒绝几何/映射组件和层会话。

所有任务包括纯计算都占用模型写权到真实终态，undo/redo 与其他模型写在此期间拒绝。masked 只控制界面遮罩。compute 的 report 是进度心跳和取消检查；GUI write 的 report 只展示。成功计算返回后、提交开始前也检查取消；提交开始后不再中断。无心跳计算无法及时取消，job->cancel() 只是请求停止。功能退出按 owner 取消，框架等待计算结束后退出清理；插件不要自行拆除 Handler 或自建作废令牌。

## 视口交互

先读取 InteractionState.h 与 InteractionContext.h。onPick/onHover 在渲染线程运行，可在回调中写 annotations 供渲染层拉取；GUI 不直接改交互状态与标注。GUI 用 requestRefresh() 或 deferRefresh(op) 交给渲染线程同步消费。deactivate 先于交互下线，延迟清理会在 clearSession 前执行；按目标状态幂等应用启停。

交互能力、结果和环境分别走声明、事件、上下文，不依赖 app 层，不按功能名要求修改通用界面。

## 通用 Feature 分类导航

分类描述父入口，Feature 描述可执行的子功能。两者通过稳定分类 id 关联，适用于求解、几何、网格生成等业务。父入口可以拥有 0、1 或多个子功能：0 显示空态并禁用执行，1 直接显示参数，多个显示子功能选择器；均由同一操作会话管理。

参数和分类在 `setup(FeatureRegistrar&, FeatureContext&)` 中声明。`FeatureRegistrar` 收集通用注册信息，`registrar.navigation()` 返回管理导航声明的 `FeatureNavigation`；导航不从 JSON 或旁置文件解析，插件 JSON 保留宿主识别所需的元数据。

```cpp
void ExampleSolver::setup(FeatureRegistrar& registrar, FeatureContext&)
{
    registrar.addParameter({ ArgTypeEnum::Int, "迭代数", "100" });
    auto& navigation = registrar.navigation();
    navigation.addCategory({ "analysis", "分析", "", 20, "功能/求解" });
    navigation.setLabel("稳态求解器");
    navigation.setOrder(10);
    navigation.setCategoryDefault("analysis", "迭代数", "200");
}
```

`FeatureCategory` 的字段顺序为 `id`、`title`、`icon`、`order`、`menu_path`。id 为稳定身份，title 必填；icon、menu_path 默认空，order 默认 0。menu_path 使用“菜单/分组”结构，例如“功能/求解”；插件留空时继承宿主同 id 的路径。解析后的路径仍为空时，通用界面先使用首个子功能的菜单贡献；没有子功能或该贡献也为空时，使用“功能”菜单。

- `navigation.addCategory` 即时忽略空 id、保留身份 `other`、空 title 与重复 id，首个有效描述保留。`categories()` 从合法的 `categoryDefinitions()` 派生，查询不暴露可变容器。一个 Feature 可加入多个分类。
- 同 id 的分类由 FeatureSystem 聚合。插件描述优先于宿主；多个插件冲突时选择功能唯一名按字典序最小的描述，空 menu_path 仅继承宿主同 id 的路径。分类按 order、id 排序。
- `setLabel` 为空时使用 display_name；`setGroup` 保留分组元数据，当前平面子功能选择器不按它分组，父入口在菜单中的分组由 menu_path 决定；`setIcon` 的资源由插件提供；`setOrder` 指定子功能排序。`setCategoryDefault` 按参数名匹配，Combo 使用选项索引字符串；进入分类时应用，用户随后可修改。
- 未声明有效分类的 Feature 沿用普通菜单；已声明分类的功能进入所属父入口，不重复出现在普通功能菜单。execute 从 `ctx.params` 读取参数，激活、事件、后台任务和 undo 统一走 FeatureSystem，不经旧算法/编辑系统转发。

需要没有实现也保留的父入口，由产品宿主在模型操作空闲时配置。例如宿主初始化时：

```cpp
feature_system.setNavigationCategories({
    { "analysis", "分析", "", 20, "功能/求解" }
});
```

`setNavigationCategories` 替换宿主声明并通知界面，使用与插件相同的分类校验。FeatureSystem 默认没有固定分类；宿主入口不会因最后一个插件卸载而消失，未被宿主保留的入口随提供者退出而消失。产品默认入口属于应用层，插件不在 setup 中改写宿主配置。

网格生成只是一个业务实例，其 setup 仍使用同一接口：

```cpp
registrar.addParameter({ ArgTypeEnum::Combo, "网格类型", "三角形,四边形" });
auto& navigation = registrar.navigation();
navigation.addCategory({ "quadrilateral", "四边形网格生成", "", 20, "网格生成算法/生成" });
navigation.setLabel("四边形网格生成（示例方法）");
navigation.setCategoryDefault("quadrilateral", "网格类型", "1");
```

空导航继续支持普通 Feature 插件，既有插件无需为导航修改源码。公共 Feature 头文件变化后必须用匹配 SDK、工具链和依赖，全量重建主程序、项目内插件、示例及独立 Addons；源码无需迁移不代表旧 DLL 保持 ABI 兼容。本次只冻结旧算法/编辑接口，不要求迁移或删除所有既有调用者。
