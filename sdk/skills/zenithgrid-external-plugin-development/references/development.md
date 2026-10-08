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

算法导航统一通过 `AlgorithmHandler::setup(AlgorithmRegistrar&)` 声明。JSON 只保留 `system`、`handler.name`、`handler.display_name` 等身份信息；不再解析 `handler.navigation` 或 DLL 旁置导航文件。

```cpp
#include "AlgorithmHandler.h"
#include "AlgorithmRegistrar.h"

class ExampleQuadMesher : public systems::algo::AlgorithmHandler {
public:
    void setup(systems::algo::AlgorithmRegistrar& registrar) override
    {
        registrar.addCategory({ "quadrilateral", "四边形网格生成", "", 20 });
        registrar.setLabel("四边形网格生成（示例方法）");
        registrar.setGroup("生成");
        registrar.setOrder(10);
        registrar.setCategoryDefault("quadrilateral", "网格类型", "1");
    }
    // 实现 args_type()、execute()，必要时覆盖 resolveComponentId()。
};
```

- `setup` 每次注册调用一次，查询、分类切换和算法执行不重复调用。默认空实现将算法归入“其他算法”。注册器仅收集声明，不持有模型或界面，不能保存其引用。
- 声明及参数准备成功后整体登记；setup 抛异常时不新增条目，替换失败保留原算法；模型忙碌时在调用 setup 前拒绝注册。
- `addCategory` 接收完整分类描述：稳定 `id`、必填 `title`、可选资源 `icon` 和排序 `order`。`other` 为保留身份。一个算法可以声明多个分类，同一 id 自动合并为同一个按钮和页面，算法身份仍使用各自的 `handler.name`。
- 同类描述应保持一致；冲突时记录警告，选择算法唯一名按字典序最小的完整声明，结果与加载顺序无关。分类按 order、id 排序。
- 三角形、四边形、四面体、六面体四个基础分类始终显示；无算法时显示“暂无可用算法”并禁用执行。扩展分类由当前注册表派生，最后一个提供者卸载后消失。
- `setLabel` 使用业务名称，空时沿用 display_name。`setIcon` 的资源由插件注册；group 为空时使用默认分组，order 越小越靠前，同值按算法身份排序。
- `setCategoryDefault` 按分类设置参数默认值，参数名与 args_type() 一致，Combo 使用选项索引的字符串；进入分类时应用，用户随后可修改。参数控件仍由宿主生成，插件不依赖 app 层。
- 直接 C++ 注册与动态、静态插件统一走 Handler 的 setup；HandlerMetaData 仅保存身份，没有第二套导航声明入口。

此改动更新 AlgorithmHandler 的虚函数接口，旧算法 DLL 不能混用。更新 SDK 后必须重建主程序、项目内插件、示例及独立 Addons 插件，并使用匹配的工具链和依赖。旧 JSON 导航配置和旁置文件应删除；分类需迁移至 setup，不提供兼容解析。
