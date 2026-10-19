/** @file TestUndoQml.cpp
 * @brief 实际 QML 删除与选择器失效路径回归测试
 */
#include "ComponentData.h"
#include "FeatureContext.h"
#include "FeatureHandler.h"
#include "FeatureRegistrar.h"
#include "FeatureSystem.h"
#include "GeometryData.h"
#include "MeshActorManager.h"
#include "MeshActorSelectOp.h"
#include "MeshData.h"
#include "ModelLayer.h"
#include "QFeatureInfo.h"
#include "QModelManager.h"
#include "QRenderWindow.h"
#include "QRenderWindowStyle.h"
#include "QSelection.h"
#include "TreeModel.h"
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickVTKItem.h>
#include <QQuickWindow>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <map>
#include <utility>
#include <vtkCamera.h>
#include <vtkCommand.h>
#include <vtkDisplaySizedImplicitPlaneRepresentation.h>
#include <vtkDisplaySizedImplicitPlaneWidget.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>

Q_IMPORT_QML_PLUGIN(app_corePlugin)
Q_IMPORT_QML_PLUGIN(app_modelPlugin)
Q_IMPORT_QML_PLUGIN(app_renderPlugin)
Q_IMPORT_QML_PLUGIN(app_model_systemsPlugin)
Q_IMPORT_QML_PLUGIN(app_model_systems_algoPlugin)
Q_IMPORT_QML_PLUGIN(app_model_systems_ioPlugin)
Q_IMPORT_QML_PLUGIN(app_model_systems_editPlugin)
Q_IMPORT_QML_PLUGIN(app_model_systems_featurePlugin)

namespace {
class ManySelectors : public systems::feature::FeatureHandler {
public:
    void setup(systems::feature::FeatureRegistrar& reg, systems::feature::FeatureContext&) override
    {
        for (int i = 0; i < 50; ++i)
            reg.addParameter({ ArgTypeEnum::Selector, "目标", "Face", "" });
    }
    std::any execute(systems::feature::FeatureContext& ctx) override
    {
        return ctx.model.addModel("undo fixture", { });
    }
};
QGuiApplication& application()
{
    // Windows 离屏平台没有 OpenGL，使用不可见原生窗口执行真实拾取回归。
#ifdef Q_OS_WIN
    qputenv("QT_QPA_PLATFORM", "windows");
#else
    qputenv("QT_QPA_PLATFORM", "offscreen");
#endif
    qputenv("QT_FORCE_STDERR_LOGGING", "1");
    QQuickVTKItem::setGraphicsApi();
    static int argc = 1;
    static char name[] = "TestUndoQml";
    static char* argv[] = { name, nullptr };
    static QGuiApplication app(argc, argv);
    return app;
}
std::unique_ptr<QObject> load(QQmlEngine& engine, const char* file, const QVariantMap& properties = { })
{
    const QUrl url(QString("qrc:/undo-ui/") + file);
    QQmlComponent component(&engine);
    if (QString(file) == "ObjectTree.qml") {
        QFile source(":" + url.path());
        REQUIRE(source.open(QIODevice::ReadOnly));
        component.setData("import UndoUi 1.0\n" + source.readAll(), url);
    } else {
        component.loadUrl(url);
    }
    INFO(component.errorString().toStdString());
    REQUIRE(component.isReady());
    std::unique_ptr<QObject> object(component.createWithInitialProperties(properties));
    INFO(component.errorString().toStdString());
    REQUIRE(object);
    return object;
}
struct QmlFixture {
    QTemporaryDir directory;
    std::string executable;
    QQmlEngine engine;
    std::unique_ptr<QObject> session;
    std::unique_ptr<QObject> sidebar;
    std::unique_ptr<QObject> tree;
    std::unique_ptr<QObject> viewport;
    QModelManager* manager;
    QObject* app;
    QObject* selection_state;

    QmlFixture()
    {
        static const int tree_type = qmlRegisterType<TreeModel>("UndoUi", 1, 0, "TreeModel");
        static const int theme_type = qmlRegisterSingletonType(QUrl("qrc:/undo-ui/Theme.qml"), "UndoUi", 1, 0, "Theme");
        REQUIRE(directory.isValid());
        executable = (directory.path() + "/isolated/Test.exe").toStdString();
        QModelManager::argv0 = executable;
        engine.rootContext()->setContextProperty("Theme", engine.singletonInstance<QObject*>(theme_type));
        session = load(engine, "OperationSession.qml");
        sidebar = load(engine, "SideBar.qml", { { "session", QVariant::fromValue(session.get()) } });
        tree = load(engine, "ObjectTree.qml");
        viewport = load(engine, "CentralRenderArea.qml");
        manager = engine.singletonInstance<QModelManager*>("app.model", "QModelManager");
        app = engine.singletonInstance<QObject*>("app.core", "App");
        REQUIRE(manager);
        REQUIRE(app);
        selection_state = app->property("selection").value<QObject*>();
        REQUIRE(selection_state);
    }
    ~QmlFixture()
    {
        app->setProperty("activeOperation", QVariant::fromValue(QJSValue(QJSValue::NullValue)));
        QModelManager::argv0 = { };
    }
};

// 等实际模型刷新完成；定时器只限制失败等待，不依赖 UI 刷新周期。
bool waitForRefresh(TreeModel& tree_model)
{
    QEventLoop loop;
    bool refreshed = false;
    QObject::connect(&tree_model, &QAbstractItemModel::modelReset, &loop, [&] {
        refreshed = true;
        loop.quit();
    });
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(2000);
    loop.exec();
    return refreshed;
}

}

TEST_CASE("Actual QML synchronizes reversed mesh and geometry visibility across tree refresh", "[QML][visibility]")
{
    application();
    QmlFixture f;
    auto& model = *f.manager->getModelManager();
    model.setUndoRecorder(nullptr);
    ComponentDatas components;
    auto component = std::make_unique<ComponentData>();
    component->mesh = std::make_unique<MeshData>();
    component->geometry = std::make_unique<GeometryData>();
    components.push_back(std::move(component));
    const Index model_id = model.addModel("visibility fixture", std::move(components));
    const Index component_id = model.modelById(model_id)->componentIds()[0];
    auto* tree = f.tree->findChild<TreeModel*>();
    auto* viewport = f.viewport->findChild<QQuickVTKItem*>();
    REQUIRE(tree);
    REQUIRE(viewport);
    REQUIRE(tree->refresh());

    for (auto [mesh_visible, geometry_visible] : { std::make_pair(true, false),
             std::make_pair(false, false), std::make_pair(false, true), std::make_pair(true, true) }) {
        // 触发实际视口信号，覆盖 CentralRenderArea 到 ObjectTree 的回传链路。
        REQUIRE(QMetaObject::invokeMethod(viewport, "componentVisibilityChanged",
            Q_ARG(Index, component_id), Q_ARG(bool, mesh_visible), Q_ARG(bool, geometry_visible)));
        for (int round = 0; round < 2; ++round) {
            const auto index = tree->findIndexByNodeId(component_id, 1);
            REQUIRE(index.isValid());
            CHECK(tree->data(index, TreeModel::IsVisibleRole).toBool() == (mesh_visible || geometry_visible));
            CHECK(tree->data(tree->parent(index), TreeModel::IsVisibleRole).toBool() == (mesh_visible || geometry_visible));
            REQUIRE(tree->rowCount(index) == 2);
            for (int i = 0; i < tree->rowCount(index); ++i) {
                const auto child = tree->index(i, 0, index);
                const bool is_mesh = tree->data(child, TreeModel::NodeTypeRole).toInt() == static_cast<int>(TreeNode::NodeType::Mesh);
                CHECK(tree->data(child, TreeModel::IsVisibleRole).toBool() == (is_mesh ? mesh_visible : geometry_visible));
            }
            // 树重建后仍需保留各子节点状态，不能退回统一的组件开关。
            REQUIRE(tree->refresh());
        }
    }
}

//! @brief 验证全场景恢复入口，以及模型、组件、网格菜单的恢复范围。
TEST_CASE("Actual QML show all entries restore hidden faces and end isolation", "[QML][visibility]")
{
    application();
    QmlFixture f;
    auto& model = *f.manager->getModelManager();
    model.setUndoRecorder(nullptr);
    ComponentDatas components;
    auto component = std::make_unique<ComponentData>();
    component->mesh = std::make_unique<MeshData>();
    auto& mesh = *component->mesh;
    mesh.vertex_positions_ = { { 0.0, 0.0, 0.0 }, { 1.0, 0.0, 0.0 }, { 1.0, 1.0, 0.0 }, { 0.0, 1.0, 0.0 } };
    mesh.vertex_count_ = 4;
    mesh.face_vertices_ = { 0, 1, 2, 0, 2, 3 };
    mesh.face_vertices_offset_ = { 0, 3, 6 };
    components.push_back(std::move(component));
    auto second_component = std::make_unique<ComponentData>();
    second_component->mesh = mesh.clone();
    for (auto& point : second_component->mesh->vertex_positions_)
        point[0] += 1.5;
    components.push_back(std::move(second_component));
    const Index model_id = model.addModel("show all fixture", std::move(components));
    const auto component_ids = model.modelById(model_id)->componentIds();
    const Index id = component_ids[0];
    const Index second_id = component_ids[1];
    auto* tree = f.tree->findChild<TreeModel*>();
    auto* viewport = f.viewport->findChild<QRenderWindow*>();
    REQUIRE(tree);
    REQUIRE(viewport);
    REQUIRE(tree->refresh());
    // 正式加载和真实面选择准备隔离状态，不向窗口注入测试专用管线。
    for (Index component_id : component_ids) {
        auto data = f.manager->getModelQuery()->getMeshDataByComponent(component_id);
        REQUIRE(data);
        viewport->loadMeshData(component_id, *data);
    }
    QQuickWindow host;
    host.resize(900, 600);
    host.setFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
    host.setOpacity(0.0);
    host.setPersistentSceneGraph(false);
    host.setPersistentGraphics(false);
    auto* page = qobject_cast<QQuickItem*>(f.viewport.get());
    REQUIRE(page);
    page->setParentItem(host.contentItem());
    page->setSize(QSizeF(600, 600));
    auto* tree_page = qobject_cast<QQuickItem*>(f.tree.get());
    REQUIRE(tree_page);
    tree_page->setParentItem(host.contentItem());
    tree_page->setX(600);
    tree_page->setSize(QSizeF(300, 600));
    const auto cleanup = qScopeGuard([&] {
        host.hide();
        host.releaseResources();
        QCoreApplication::processEvents();
        page->setParentItem(nullptr);
        tree_page->setParentItem(nullptr);
    });
    // 逐轮消费渲染命令及 GUI 快照回传，保证后续操作使用已经送达的选择。
    auto flush = [&] {
        QElapsedTimer timer;
        timer.start();
        for (int round = 0; round < 8; ++round) {
            auto completed = std::make_shared<std::atomic_bool>(false);
            viewport->dispatch_async([completed](vtkRenderWindow*, QQuickVTKItem::vtkUserData) { completed->store(true); });
            while (!completed->load() && timer.elapsed() < 5000) {
                QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
                QThread::msleep(1);
            }
            if (!completed->load())
                return false;
            QCoreApplication::processEvents();
        }
        return true;
    };
    viewport->setSelectMode("Face");
    host.show();
    REQUIRE(flush());
    // 两个组件分别真实拾取原面 0，再隔离为单面，供所有恢复入口共用。
    auto isolate_face = [&](Index component_id, double x) {
        viewport->dispatch_async([x](vtkRenderWindow* render_window, QQuickVTKItem::vtkUserData user_data) {
            auto* scene = QRenderWindow::Data::SafeDownCast(user_data);
            auto* camera = scene->renderer_->GetActiveCamera();
            camera->SetPosition(1.25, 0.5, 5.0);
            camera->SetFocalPoint(1.25, 0.5, 0.0);
            camera->SetViewUp(0.0, 1.0, 0.0);
            camera->SetParallelScale(1.5);
            scene->renderer_->ResetCameraClippingRange();
            scene->renderer_->SetWorldPoint(x, 0.25, 0.0, 1.0);
            scene->renderer_->WorldToDisplay();
            double display[3] { };
            scene->renderer_->GetDisplayPoint(display);
            render_window->GetInteractor()->SetEventPosition(
                static_cast<int>(std::lround(display[0])), static_cast<int>(std::lround(display[1])));
            scene->style_->SetClick();
            scene->style_->OnLeftButtonUp();
        });
        REQUIRE(flush());
        std::unique_ptr<QSelection> selection(viewport->selectedIDs());
        REQUIRE(selection);
        REQUIRE(selection->get()->component_id == component_id);
        REQUIRE(selection->get()->ids == std::vector<Index>({ 0 }));
        viewport->isolateSelectedFaces();
        REQUIRE(flush());
    };
    // 在渲染线程读取实际面集与整体开关，区分面恢复和组件显示。
    auto check_mesh = [&](Index component_id, vtkIdType expected_faces, bool expected_visible) {
        vtkIdType face_count = -1;
        bool visible = false;
        bool ordinary_style = false;
        viewport->dispatch_async([&face_count, &visible, &ordinary_style, component_id](vtkRenderWindow*, QQuickVTKItem::vtkUserData user_data) {
            auto* scene = QRenderWindow::Data::SafeDownCast(user_data);
            auto actor = scene->mesh_actor_manager_->getComponentActor(component_id);
            if (!actor)
                return;
            MeshActorSelectOp select_op(actor);
            auto* face_actor = vtkActor::SafeDownCast(&select_op.getFaceActor());
            auto* mapper = face_actor ? vtkPolyDataMapper::SafeDownCast(face_actor->GetMapper()) : nullptr;
            if (mapper) {
                mapper->Update();
                face_count = mapper->GetInput()->GetNumberOfCells();
            }
            visible = actor->isVisibilityEnabled();
            ordinary_style = select_op.getEdgeActor().GetVisibility();
        });
        REQUIRE(flush());
        CAPTURE(component_id);
        CHECK(face_count == expected_faces);
        CHECK(visible == expected_visible);
        CHECK(ordinary_style == expected_visible);
    };
    isolate_face(id, 0.75);
    isolate_face(second_id, 2.25);
    check_mesh(id, 1, true);
    check_mesh(second_id, 1, true);
    viewport->setMeshVisibility(id, false);
    REQUIRE_FALSE(tree->data(tree->findIndexByNodeId(id, 1), TreeModel::IsVisibleRole).toBool());

    QString restore_entry;
    Index other_id = -1;
    SECTION("object tree toolbar")
    {
        REQUIRE(QMetaObject::invokeMethod(f.tree.get(), "showAllNodes"));
    }
    SECTION("viewport context menu")
    {
        auto* menu = f.viewport->findChild<QObject*>("viewportVisibilityMenu");
        REQUIRE(menu);
        QObject* show_all = nullptr;
        for (auto* item : menu->findChildren<QObject*>()) {
            if (item->property("shown").isValid() && item->property("text").toString() == "全部显示")
                show_all = item;
        }
        REQUIRE(show_all);
        REQUIRE(QMetaObject::invokeMethod(show_all, "triggered"));
    }
    SECTION("object tree component face restore") { restore_entry = QString("restoreHiddenFaces-1-%1").arg(id); }
    SECTION("object tree mesh face restore") { restore_entry = QString("restoreHiddenFaces-2-%1").arg(id); }
    SECTION("object tree model face restore")
    {
        restore_entry = QString("restoreHiddenFaces-0-%1").arg(model_id);
        ComponentDatas other_components;
        auto other_component = std::make_unique<ComponentData>();
        other_component->mesh = mesh.clone();
        other_components.push_back(std::move(other_component));
        const Index other_model_id = model.addModel("unrelated fixture", std::move(other_components));
        other_id = model.modelById(other_model_id)->componentIds()[0];
        REQUIRE(tree->refresh());
        auto data = f.manager->getModelQuery()->getMeshDataByComponent(other_id);
        REQUIRE(data);
        viewport->loadMeshData(other_id, *data);
        // A 整体隐藏后，在同一位置拾取其他模型的面，验证恢复不会越过模型边界。
        isolate_face(other_id, 0.75);
        check_mesh(other_id, 1, true);
        viewport->setMeshClip(true);
        viewport->dispatch_async([](vtkRenderWindow*, QQuickVTKItem::vtkUserData user_data) {
            auto* scene = QRenderWindow::Data::SafeDownCast(user_data);
            auto* plane = scene->plane_widget_->GetDisplaySizedImplicitPlaneRepresentation();
            // 裁掉目标模型的第二个组件，恢复隐藏面后它仍应受该裁剪约束。
            plane->SetOrigin(1.25, 0.0, 0.0);
            plane->SetNormal(1.0, 0.0, 0.0);
            scene->plane_widget_->InvokeEvent(vtkCommand::InteractionEvent);
        });
        REQUIRE(flush());
        check_mesh(second_id, 0, true);
    }
    if (!restore_entry.isEmpty()) {
        // 展开并布局真实对象树 delegate，再触发对应节点实际创建的菜单项。
        auto* tree_view = f.tree->findChild<QObject*>("objectTreeView");
        REQUIRE(tree_view);
        REQUIRE(QMetaObject::invokeMethod(tree_view, "expandRecursively", Q_ARG(int, -1), Q_ARG(int, -1)));
        REQUIRE(QMetaObject::invokeMethod(tree_view, "forceLayout"));
        REQUIRE(flush());
        auto node_index = tree->findIndexByNodeId(id, 1);
        const bool restore_model = restore_entry.startsWith("restoreHiddenFaces-0-");
        if (restore_model)
            node_index = node_index.parent();
        else if (restore_entry.startsWith("restoreHiddenFaces-2-"))
            node_index = tree->index(0, 0, node_index);
        QQuickItem* delegate = nullptr;
        REQUIRE(QMetaObject::invokeMethod(tree_view, "itemAtIndex",
            Q_RETURN_ARG(QQuickItem*, delegate), Q_ARG(QModelIndex, node_index)));
        REQUIRE(delegate);
        auto* restore = delegate->findChild<QObject*>(restore_entry);
        REQUIRE(restore);
        CHECK(restore->property("text").toString() == "恢复隐藏面");
        auto* menu = restore->property("menu").value<QObject*>();
        REQUIRE(menu);
        REQUIRE(QMetaObject::invokeMethod(menu, "open"));
        REQUIRE(flush());
        CHECK(restore->property("visible").toBool());
        CHECK(restore->property("height").toDouble() > 0);
        REQUIRE(QMetaObject::invokeMethod(restore, "triggered"));
        REQUIRE(QMetaObject::invokeMethod(menu, "close"));
        CHECK_FALSE(tree->data(tree->findIndexByNodeId(id, 1), TreeModel::IsVisibleRole).toBool());
        check_mesh(id, 2, false);
        check_mesh(second_id, restore_model ? 0 : 1, true);
        if (restore_model) {
            check_mesh(other_id, 1, true);
            // 停用裁剪后，同模型两个组件均恢复完整面集，其他模型仍保留隐藏面。
            viewport->setMeshClip(false);
            check_mesh(id, 2, false);
            check_mesh(second_id, 2, true);
            check_mesh(other_id, 1, true);
        }
    } else {
        CHECK(tree->data(tree->findIndexByNodeId(id, 1), TreeModel::IsVisibleRole).toBool());
        check_mesh(id, 2, true);
        check_mesh(second_id, 2, true);
        // 复用当前场景验证对象树反转入口，避免单独构建一套 QML 场景。
        REQUIRE(QMetaObject::invokeMethod(f.tree.get(), "reverseDisplayed"));
        CHECK_FALSE(tree->data(tree->findIndexByNodeId(id, 1), TreeModel::IsVisibleRole).toBool());
    }
}

TEST_CASE("Actual QML aligns face and component visibility menu entries", "[QML][visibility]")
{
    application();
    QmlFixture f;
    auto* menu = f.viewport->findChild<QObject*>("viewportVisibilityMenu");
    REQUIRE(menu);
    std::map<QString, QObject*> items;
    for (auto* item : menu->findChildren<QObject*>()) {
        if (item->property("shown").isValid()) {
            const auto text = item->property("text").toString();
            REQUIRE(items.count(text) == 0);
            items[text] = item;
        }
    }
    REQUIRE(items.size() == 6);
    for (const auto& mode : { QString("Face"), QString("Component") }) {
        f.selection_state->setProperty("selectMode", mode);
        for (bool has_picks : { false, true }) {
            // 传入真实 QSelection，覆盖 C++ 选择快照到 QML 菜单的判断路径。
            auto data = std::make_unique<Selection>();
            data->type = mode == "Face" ? ElementEnum::Face : ElementEnum::Component;
            data->component_id = 7;
            if (has_picks)
                data->ids = { 0, 1 };
            QSelection selection(std::move(data));
            REQUIRE(QMetaObject::invokeMethod(menu, "refreshSelection",
                Q_ARG(QVariant, QVariant::fromValue(&selection))));
            CHECK(menu->property("hasPicks").toBool() == has_picks);
            for (const auto& text : { QString("隐藏"), QString("显示"), QString("隔离") })
                CHECK(items.at(text)->property("shown").toBool() == has_picks);
            for (const auto& text : { QString("全部隐藏"), QString("全部显示") })
                CHECK(items.at(text)->property("shown").toBool() == !has_picks);
            CHECK(items.at("反转显示")->property("shown").toBool());
        }
    }
    // 尚未取得选择快照时也应恢复全局操作，不能保留上次菜单的选择状态。
    f.selection_state->setProperty("selectMode", "Face");
    REQUIRE(QMetaObject::invokeMethod(menu, "refreshSelection",
        Q_ARG(QVariant, QVariant::fromValue(static_cast<QObject*>(nullptr)))));
    CHECK_FALSE(menu->property("hasPicks").toBool());
    for (const auto& text : { QString("隐藏"), QString("显示"), QString("隔离") })
        CHECK_FALSE(items.at(text)->property("shown").toBool());
}

TEST_CASE("Actual QML follows successful deletion notifications and preserves selection on refusal", "[QML][undo]")
{
    application();
    QmlFixture f;
    auto* manager = f.manager;
    auto* selection_state = f.selection_state;
    auto& tree = f.tree;
    auto& model = *manager->getModelManager();
    auto* tree_model = tree->findChild<TreeModel*>();
    REQUIRE(tree_model);
    model.setUndoRecorder(nullptr);
    ComponentDatas components;
    components.push_back(std::make_unique<ComponentData>());
    components.push_back(std::make_unique<ComponentData>());
    components.push_back(std::make_unique<ComponentData>());
    const Index mid = model.addModel("fixture", std::move(components));
    const Index cid = model.modelById(mid)->componentIds()[0];
    const Index remaining_cid = model.modelById(mid)->componentIds()[1];
    const Index other_cid = model.modelById(mid)->componentIds()[2];
    selection_state->setProperty("activeModelId", mid);
    selection_state->setProperty("activeComponentId", cid);
    auto operation = model.beginWriteOperation(false);
    for (int depth = 0; depth < 2; ++depth) {
        REQUIRE(QMetaObject::invokeMethod(tree.get(), "deleteNode", Q_ARG(QVariant, depth == 0 ? mid : cid),
            Q_ARG(QVariant, depth), Q_ARG(QVariant, 0), Q_ARG(QVariant, cid)));
        CHECK(selection_state->property("activeModelId").toInt() == mid);
        CHECK(selection_state->property("activeComponentId").toInt() == cid);
        CHECK(model.findComponent(cid));
    }
    operation.reset();
    // 删除非当前项不改变选择。
    model.removeComponent(other_cid);
    CHECK(selection_state->property("activeModelId").toInt() == mid);
    CHECK(selection_state->property("activeComponentId").toInt() == cid);
    const Index other_mid = model.addModel("other", { });
    model.removeModel(other_mid);
    CHECK(selection_state->property("activeModelId").toInt() == mid);
    CHECK(selection_state->property("activeComponentId").toInt() == cid);
    // 直接修改模型层，验证界面响应实际删除通知，独立于对象树命令入口。
    model.removeComponent(cid);
    CHECK(selection_state->property("activeModelId").toInt() == mid);
    CHECK(selection_state->property("activeComponentId").toInt() == remaining_cid);
    REQUIRE(waitForRefresh(*tree_model));
    CHECK(selection_state->property("activeModelId").toInt() == mid);
    CHECK(selection_state->property("activeComponentId").toInt() == remaining_cid);
    // 删除最后一个组件仍保留模型选择。
    model.removeComponent(remaining_cid);
    CHECK(selection_state->property("activeModelId").toInt() == mid);
    CHECK(selection_state->property("activeComponentId").toInt() == -1);
    REQUIRE(waitForRefresh(*tree_model));
    CHECK(selection_state->property("activeModelId").toInt() == mid);
    CHECK(selection_state->property("activeComponentId").toInt() == -1);
    model.removeModel(mid);
    CHECK(selection_state->property("activeModelId").toInt() == -1);
    CHECK(selection_state->property("activeComponentId").toInt() == -1);
    REQUIRE(waitForRefresh(*tree_model));
    CHECK(selection_state->property("activeModelId").toInt() == -1);
    CHECK(selection_state->property("activeComponentId").toInt() == -1);
    // 整体删除带组件的当前模型同样清空选择。
    ComponentDatas model_components;
    model_components.push_back(std::make_unique<ComponentData>());
    model_components.push_back(std::make_unique<ComponentData>());
    const Index selected_mid = model.addModel("selected", std::move(model_components));
    selection_state->setProperty("activeModelId", selected_mid);
    selection_state->setProperty("activeComponentId", model.modelById(selected_mid)->componentIds()[0]);
    model.removeModel(selected_mid);
    CHECK(selection_state->property("activeModelId").toInt() == -1);
    CHECK(selection_state->property("activeComponentId").toInt() == -1);
    REQUIRE(waitForRefresh(*tree_model));
    CHECK(selection_state->property("activeModelId").toInt() == -1);
    CHECK(selection_state->property("activeComponentId").toInt() == -1);
}

TEST_CASE("Actual undo clears offscreen selectors only after successful restoration", "[QML][undo]")
{
    application();
    QmlFixture f;
    auto* manager = f.manager;
    auto* app = f.app;
    auto* selection_state = f.selection_state;
    auto& model = *manager->getModelManager();
    auto& system = *manager->getFeatureSystemAdaptor()->featureSystem();
    systems::feature::HandlerMetaData meta;
    meta.name = "ManySelectors";
    REQUIRE(system.registerHandler(meta, systems::feature::FeatureSystem::SystemHandlerPtr { std::make_unique<ManySelectors>().release() }));
    auto* info = manager->getFeatureSystemAdaptor()->getFeaturesInfo().front();
    QJSValue active = f.engine.newObject();
    // 普通功能也使用统一入口身份；视口外参数仍由同一会话模型清理。
    active.setProperty("entryId", QStringLiteral("feature:ManySelectors:功能"));
    active.setProperty("isFeature", true);
    active.setProperty("info", f.engine.newQObject(info));
    app->setProperty("activeOperation", QVariant::fromValue(active));
    QSelection selection(std::make_unique<Selection>());
    selection.get()->ids = { 123 };
    selection.get()->type = ElementEnum::Face;
    for (int i = 0; i < 50; ++i)
        REQUIRE(manager->getFeatureSystemAdaptor()->setParameter("ManySelectors", i, QVariant::fromValue(&selection)));
    selection_state->setProperty("listeningSelectorIndex", 49);
    const auto revision = selection_state->property("selectionRevision").toInt();
    auto* undo = manager->getUndoStackAdaptor();
    int applied = 0;
    QObject::connect(undo, &QUndoStackAdaptor::applied, [&] { ++applied; });
    // 空历史和忙时拒绝都不得触发选择清理。
    undo->undo();
    CHECK(applied == 0);
    const Index mid = std::any_cast<Index>(system.invoke("ManySelectors"));
    REQUIRE(model.modelById(mid));
    REQUIRE(undo->canUndo());
    auto operation = model.beginWriteOperation(false);
    undo->undo();
    CHECK(applied == 0);
    CHECK(model.modelById(mid));
    CHECK(selection_state->property("listeningSelectorIndex").toInt() == 49);
    CHECK(selection_state->property("selectionRevision").toInt() == revision);
    for (int i = 0; i < 50; ++i)
        CHECK(*system.params("ManySelectors")->value(i).get<ArgTypeEnum::Selector>());
    operation.reset();
    undo->undo();
    CHECK(applied == 1);
    CHECK_FALSE(model.modelById(mid));
    CHECK(selection_state->property("listeningSelectorIndex").toInt() == -1);
    CHECK(selection_state->property("selectionRevision").toInt() > revision);
    for (int i = 0; i < 50; ++i)
        CHECK_FALSE(*system.params("ManySelectors")->value(i).get<ArgTypeEnum::Selector>());
    // redo 同样经真实恢复路径触发清理。
    REQUIRE(undo->canRedo());
    REQUIRE(manager->getFeatureSystemAdaptor()->setParameter("ManySelectors", 49, QVariant::fromValue(&selection)));
    selection_state->setProperty("listeningSelectorIndex", 49);
    undo->redo();
    CHECK(applied == 2);
    CHECK(model.modelById(mid));
    CHECK(selection_state->property("listeningSelectorIndex").toInt() == -1);
    CHECK_FALSE(*system.params("ManySelectors")->value(49).get<ArgTypeEnum::Selector>());
}
