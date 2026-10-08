# 插件 API 与生命周期

以所导入 SDK 的头文件为最终依据：`include/precess/model/systems/{core,feature,algo,edit,io,job}`、`model/ops`、`model/data` 和 `model/session`。SDK 处于预览阶段，API 可发生不兼容变化，不把旧示例签名当作稳定承诺。

## 注册、参数和目标

- 插件继承 QObject 和 systems::PluginBase，用 HandlerCreatorDestroyerFactory 由插件创建/销毁 Handler，保持 DLL 边界两侧分配释放配对。四类 Handler 分别为 FeatureHandler、AlgorithmHandler、EditHandler、ModelIOHandler；先查头文件确认当前纯虚接口。
- JSON 的 system 分别为 FeatureSystem、AlgorithmSystem、EditSystem、ModelIOSystem；handler.name 唯一。视口交互能力通过 JSON 的 interactive 声明，宿主读取声明，不能要求通用界面按插件名特判。
- 功能 setup(FeatureRegistrar&, FeatureContext&) 注册参数、菜单、按键和订阅；activate/deactivate 对应重复进出功能；teardown(FeatureContext&) 对应注销。上下文固定绑定系统与 owner，服务始终可调用；查询或任务结果可能为空，不检测接口是否装配。
- `ctx.events.subscribe<ParameterChangedEvent>` 自动按所属功能过滤；全局监听明确使用 ctx.events.bus()。订阅句柄须保活并随生命周期退订。Button 参数只按 param_index 处理，计数载荷不作为值。
- 优先注册 Selector 让用户选择目标；活动对象树组件只作 fallback。全局点 gid 经 ModelLayer::pointIdMap() 解析组件，局部边/面选择携 component_id。算法覆盖 resolveComponentId，编辑 execute 接收 ModelLayer 与 fallback_component_id。不要把例子的活动组件简化带入正式功能。

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

## 算法导航分类声明

算法插件在 JSON 的 `handler.navigation.categories` 中声明二级分类。完整对象包含 `id`、`title`、可选 `icon`、`order`；例如 `[{"id":"quadrilateral","title":"四边形网格生成","icon":"qrc:/myplugin/quad.svg","order":20}]`。插件必须自行注册其 qrc 资源。

相同分类 `id` 自动合并到一个按钮和页面；算法 `handler.name` 保持各自唯一。`navigation.label` 是三级算法的业务名称，`navigation.order` 是算法排序，均独立于分类对象的展示信息。相同分类应提供一致的描述；冲突时宿主告警，选择算法唯一名最小的显式声明，不依赖加载顺序。

旧四类字符串声明继续兼容。自定义类别必须使用完整对象；`other` 为保留身份。未声明有效分类的算法进入“其他算法”，最后一个提供者卸载后自定义类别自动消失。运行中注册或卸载仍受模型任务占用约束。

动态插件也可用 DLL 旁同名 `.navigation.json` 提供上述 navigation 对象；该文件整体覆盖内嵌导航声明，不改变插件身份或执行接口，修改后重新加载生效。分类默认参数仍使用 `category_defaults`，键为分类 `id`。

此能力不需要插件依赖 app 层；参数继续通过 `AlgorithmHandler::args_type()` 声明，由宿主生成通用控件。
