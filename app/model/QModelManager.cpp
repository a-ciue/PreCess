#include "QModelManager.h"
#include "AlgorithmSystem.h"
#include "EditSystem.h"
#include "FeatureEvents.h"
#include "FeatureSystem.h"
#include "ModelIOSystem.h"
#include "ModelLayer.h"
#include "QModelObserver.h"
#include "QPythonRuntime.h"
#include "Session.h"
#include "SystemPluginManager.h"
#include "UndoStack.h"

#include <QUrl>
#include <filesystem>
#include <spdlog/spdlog.h>

QModelManager::QModelManager(std::string_view argv0, QObject* parent)
    : QObject(parent)
{
    // 1) 会话组合根：模型层 + undo 栈 + 事件总线 + 四系统（内部经转发观察者桥接 ModelEvent）
    observer_ = std::make_unique<QModelObserver>();
    connect(observer_.get(), &QModelObserver::modelRemoved, this, &QModelManager::modelRemoved);
    session_ = std::make_unique<session::Session>(observer_.get(), [this](std::function<void()> fn) {
        QMetaObject::invokeMethod(this, std::move(fn));
    });

    // 产品默认入口由应用声明；无实现时仍显示空态，通用功能系统不内置网格类别。
    session_->featureSystem().setNavigationCategories({ { "triangle", "三角形网格生成", "qrc:/images/toolbar/Mesh/triangle-meshing.svg", 10, "网格生成算法" },
        { "quadrilateral", "四边形网格生成", "qrc:/images/toolbar/Mesh/quad-meshing.svg", 20, "网格生成算法" },
        { "tetrahedron", "四面体网格生成", "qrc:/images/toolbar/Algorithm/tetgen.svg", 30, "网格生成算法" },
        { "hexahedron", "六面体网格生成", "qrc:/images/toolbar/Mesh/hexa-meshing.svg", 40, "网格生成算法" } });

    query_ = std::make_unique<QModelQuery>(&session_->query(), this);

    // 2) 共享任务基建：全局单槽执行器（算法与功能任务共用一个坑）+ 状态栏统一数据源。
    // 提交闭包经本对象（GUI 线程）分发：工作线程投递时为 Queued，事件循环空闲即可执行
    task_status_ = std::make_unique<QTaskStatus>();
    // 3) Qt 适配器包装会话内子系统
    algo_adaptor_ = std::make_unique<systems::algo::QAlgorithmSystemAdaptor>(session_->algorithmSystem(), *task_status_);
    io_adaptor_ = std::make_unique<systems::io::QModelIOSystemAdaptor>(session_->ioSystem());
    edit_adaptor_ = std::make_unique<systems::edit::QEditSystemAdaptor>(session_->editSystem());
    feature_adaptor_ = std::make_unique<systems::feature::QFeatureSystemAdaptor>(session_->featureSystem());
    undo_adaptor_ = std::make_unique<QUndoStackAdaptor>(session_->undoStack());
    task_status_->bindSession(*session_);

    // 功能上下文的活动模型/组件由 UI 同步到适配器，功能经 provider 动态获取；
    session_->featureSystem().setActiveModelProvider([this]() { return feature_adaptor_->activeModel(); });
    session_->featureSystem().setActiveComponentProvider([this]() { return feature_adaptor_->activeComponent(); });

    // 3) UI 桥接订阅（显示层关注点，保留在 app 层）
    // 参数变更桥接：功能回写参数值（如交互结果）经事件总线转发到 QML 同步显示
    param_bridge_sub_ = session_->events().subscribe<systems::feature::ParameterChangedEvent>(
        [this](const systems::feature::ParameterChangedEvent& e) {
            feature_adaptor_->notifyParameterChanged(e.feature, e.param_index, e.value);
        });

    // 标量属性显示桥接：功能请求经 Qt 排队信号转发，保证模型操作边界 flush 后再设置渲染属性。
    scalar_attribute_display_bridge_sub_
        = session_->events().subscribe<systems::feature::ScalarAttributeDisplayRequestedEvent>(
            [this](const systems::feature::ScalarAttributeDisplayRequestedEvent& event) {
                feature_adaptor_->notifyScalarAttributeDisplayRequested(
                    event.component_id, event.attribute_name);
            });

    q_plugin_manager_ = std::make_unique<systems::QSystemPluginManager>(&session_->pluginManager());

    // 4) 内嵌 Python 运行时：懒初始化（控制台首次使用时启动解释器），持有活会话引用
    python_runtime_ = std::make_unique<QPythonRuntime>(session_.get(), this);

    // 5) 注册插件：静态插件经适配器注册以同步 QML 插件名列表
    q_plugin_manager_->registerStaticPlugins();
#ifndef __EMSCRIPTEN__
    using std::filesystem::path;
    path exe_dir = std::filesystem::absolute(argv0).parent_path();
    path plugin_dir = exe_dir / "plugins"; // 对应 开发调试 时的目录结构，相对严格
    if (!std::filesystem::is_directory(plugin_dir)) {
        plugin_dir = exe_dir / "../plugins"; // 对应 install 后的目录结构，相对宽松
    }
    if (!std::filesystem::is_directory(plugin_dir)) {
        spdlog::info("QModelManager::QModelManager: 插件目录 {} 不存在，跳过动态插件加载", plugin_dir.string());
        return;
    }
    // 遍历插件目录，加载所有插件（经适配器注册以同步 QML 插件名列表）
    for (const auto& entry : std::filesystem::directory_iterator(plugin_dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const path& entry_path = entry.path();
        const path extension = entry_path.extension();
        if (extension != ".dll" && extension != ".so" && extension != ".dylib") {
            continue;
        }
        QUrl plugin_url = QUrl::fromLocalFile(QString::fromLocal8Bit(entry_path.string()));
        q_plugin_manager_->registerPlugin(plugin_url);
    }
#endif
}

QModelManager::~QModelManager()
{
    // 0) 先停全局单槽执行器：等待在飞任务自然结束、拒收新任务——此后任何回调不再触发，
    //    适配器/会话随后析构均无并发回调悬垂问题
    session_->stopJobs();
    // 先关 Python 运行时（丢弃 precess.current 活会话引用并终结解释器）：
    // Python 侧以引用策略持有活会话，会话先析构会在终结前留下悬垂
    python_runtime_.reset();
    // 显式再拆会话（停功能系统、断 undo 钩子）：此刻 Qt 适配器均存活，
    // 层清理路径经 on_changed_ 回调 undo_adaptor_ 发信号安全；其余成员按声明逆序析构
    session_.reset();
}

void QModelManager::removeModel(int id)
{
    try {
        session_->removeModel(id);
    } catch (const ModelOperationBusy& e) {
        spdlog::warn("QModelManager: {}", e.what());
    }
}

void QModelManager::removeComponent(int id)
{
    try {
        session_->removeComponent(id);
    } catch (const ModelOperationBusy& e) {
        spdlog::warn("QModelManager: {}", e.what());
    }
}

void QModelManager::removeMesh(int componentId)
{
    try {
        session_->removeMesh(componentId);
    } catch (const ModelOperationBusy& e) {
        spdlog::warn("QModelManager: {}", e.what());
    }
}

void QModelManager::removeGeometry(int componentId)
{
    try {
        session_->removeGeometry(componentId);
    } catch (const ModelOperationBusy& e) {
        spdlog::warn("QModelManager: {}", e.what());
    }
}

ModelLayer* QModelManager::getModelManager()
{
    return &session_->model();
}

QModelObserver* QModelManager::getModelObserver() const
{
    return observer_.get();
}

QModelQuery* QModelManager::getModelQuery() const
{
    return query_.get();
}

systems::algo::QAlgorithmSystemAdaptor* QModelManager::getAlgorithmSystemAdaptor() const
{
    return algo_adaptor_.get();
}

systems::edit::QEditSystemAdaptor* QModelManager::getEditSystemAdaptor() const
{
    return edit_adaptor_.get();
}

systems::io::QModelIOSystemAdaptor* QModelManager::getModelIOSystemAdaptor() const
{
    return io_adaptor_.get();
}

systems::feature::QFeatureSystemAdaptor* QModelManager::getFeatureSystemAdaptor() const
{
    return feature_adaptor_.get();
}

systems::QSystemPluginManager* QModelManager::getSystemPluginManager() const
{
    return q_plugin_manager_.get();
}

QUndoStackAdaptor* QModelManager::getUndoStackAdaptor() const
{
    return undo_adaptor_.get();
}

QPythonRuntime* QModelManager::getPythonRuntime() const
{
    return python_runtime_.get();
}

QTaskStatus* QModelManager::getTaskStatus() const
{
    return task_status_.get();
}

#ifdef _WIN32
std::string_view QModelManager::argv0 = "./PreCess.exe";
#else
std::string_view QModelManager::argv0 = "./PreCess";
#endif

QModelManager* QModelManager::create(QQmlEngine*, QJSEngine*)
{
    return new QModelManager(argv0);
}
