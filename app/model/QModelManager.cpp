#include "QModelManager.h"
#include "AlgorithmSystem.h"
#include "AlgorithmSystemRegister.h"
#include "EditSystem.h"
#include "EditSystemRegister.h"
#include "GeometryBuilder.h"
#include "GeometryData.h"
#include "ModelIOSystem.h"
#include "ModelIOSystemRegister.h"
#include "ModelLayer.h"
#include "QModelObserver.h"
#include "QSelection.h"
#include "SystemPluginManager.h"
#include "ComponentData.h"

#include <QUrl>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Vertex.hxx>
#include <algorithm>
#include <filesystem>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <utility>

QModelManager::QModelManager(std::string_view argv0, QObject* parent)
    : QObject(parent)
{
    // 1) 初始化
    observer_ = std::make_unique<QModelObserver>();
    core_ = std::make_unique<ModelLayer>(
        /*observer=*/observer_.get());

    query_ = std::make_unique<QModelQuery>(core_.get(), this);

    io_system_ = std::make_unique<systems::io::ModelIOSystem>(*core_);
    algo_system_ = std::make_unique<systems::algo::AlgorithmSystem>(*io_system_, *core_);
    edit_system_ = std::make_unique<systems::edit::EditSystem>(*core_);

    algo_adaptor_ = std::make_unique<systems::algo::QAlgorithmSystemAdaptor>(*algo_system_);
    io_adaptor_ = std::make_unique<systems::io::QModelIOSystemAdaptor>(*io_system_);
    edit_adaptor_ = std::make_unique<systems::edit::QEditSystemAdaptor>(*edit_system_);

    plugin_manager_ = std::make_unique<systems::SystemPluginManager>();

    // 2) 注册系统
    plugin_manager_->addSystemRegister(systems::io::ModelIOSystem::name, std::make_unique<systems::io::ModelIOSystemRegister>(*io_system_));
    plugin_manager_->addSystemRegister(systems::algo::AlgorithmSystem::name, std::make_unique<systems::algo::AlgorithmSystemRegister>(*algo_system_));
    plugin_manager_->addSystemRegister(systems::edit::EditSystem::name, std::make_unique<systems::edit::EditSystemRegister>(*edit_system_));

    q_plugin_manager_ = std::make_unique<systems::QSystemPluginManager>(plugin_manager_.get());

    // 3) 注册插件
    using std::filesystem::path;
    path exe_dir = std::filesystem::absolute(argv0).parent_path();
    path plugin_dir = exe_dir / "plugins"; // 对应 开发调试 时的目录结构，相对严格
    if (!std::filesystem::is_directory(plugin_dir)) {
        plugin_dir = exe_dir / "../plugins"; // 对应 install 后的目录结构，相对宽松
    }
    if (!std::filesystem::is_directory(plugin_dir)) {
        spdlog::error("QModelManager::QModelManager: 插件目录 {} 不存在", plugin_dir.string());
        return;
    }
    // 遍历插件目录，加载所有插件
    for (const auto& entry : std::filesystem::directory_iterator(plugin_dir)) {
        if (entry.is_regular_file()) {
            QUrl plugin_url = QUrl::fromLocalFile(QString::fromLocal8Bit(entry.path().string()));
            getSystemPluginManager()->registerPlugin(plugin_url);
        }
    }
}

QModelManager::~QModelManager() = default;

void QModelManager::removeModel(int id)
{
    core_->removeModel(id);
    emit modelRemoved(id);
}

void QModelManager::removeComponent(int id)
{
    core_->removeComponent(id);
}

int QModelManager::createPoint(
    int modelId, int componentId, double x, double y, double z)
{
    try {
        TopoDS_Shape shape = GeometryBuilder::makePoint(x, y, z);
        const std::string component_name = "Point_" + std::to_string(next_point_number_);
        const Index result_component_id = addGeometryShape(
            modelId, componentId, component_name, std::move(shape));
        if (componentId < 0)
            ++next_point_number_;
        return result_component_id;
    } catch (const Standard_Failure& error) {
        const char* detail = error.GetMessageString();
        spdlog::error("QModelManager::createPoint: {}", detail ? detail : "OpenCASCADE error");
        emit geometryOperationFailed(
            QStringLiteral("创建 Point 失败：%1").arg(QString::fromLocal8Bit(detail ? detail : "OpenCASCADE error")));
    } catch (const std::exception& error) {
        spdlog::error("QModelManager::createPoint: {}", error.what());
        emit geometryOperationFailed(
            QStringLiteral("创建 Point 失败：%1").arg(QString::fromLocal8Bit(error.what())));
    }
    return -1;
}

int QModelManager::createLineByCoordinates(
    int modelId,
    int componentId,
    double startX,
    double startY,
    double startZ,
    double endX,
    double endY,
    double endZ)
{
    try {
        TopoDS_Shape shape = GeometryBuilder::makeLine(
            startX, startY, startZ, endX, endY, endZ);
        const std::string component_name = "Line_" + std::to_string(next_line_number_);
        const Index result_component_id = addGeometryShape(
            modelId, componentId, component_name, std::move(shape));
        if (componentId < 0)
            ++next_line_number_;
        return result_component_id;
    } catch (const Standard_Failure& error) {
        const char* detail = error.GetMessageString();
        spdlog::error("QModelManager::createLineByCoordinates: {}",
            detail ? detail : "OpenCASCADE error");
        emit geometryOperationFailed(
            QStringLiteral("创建 Line 失败：%1")
                .arg(QString::fromLocal8Bit(detail ? detail : "OpenCASCADE error")));
    } catch (const std::exception& error) {
        spdlog::error("QModelManager::createLineByCoordinates: {}", error.what());
        emit geometryOperationFailed(
            QStringLiteral("创建 Line 失败：%1").arg(QString::fromLocal8Bit(error.what())));
    }
    return -1;
}

int QModelManager::createLineFromVertices(
    int componentId, QSelection* selection)
{
    try {
        if (componentId < 0)
            throw std::invalid_argument("Select a target component first");
        if (!selection || !selection->get())
            throw std::invalid_argument("Select two geometry vertices");

        const std::shared_ptr<Selection> selected = selection->get();
        if (selected->type != ElementEnum::GeometryVertex || selected->ids.size() != 2)
            throw std::invalid_argument("Exactly two geometry vertices are required");

        ComponentData* component = core_->findComponent(componentId);
        if (!component || !component->geometry || !component->geometry->rootShape)
            throw std::invalid_argument("Target component has no geometry");
        component->geometry->ensureIndexBuilt(core_->geomRegistry());

        const auto& component_vertex_ids = component->geometry->index.vertex_local_to_global;
        for (Index id : selected->ids) {
            if (std::find(component_vertex_ids.begin(), component_vertex_ids.end(), id)
                == component_vertex_ids.end())
                throw std::invalid_argument("Selected vertices must belong to the target component");
        }

        const TopoDS_Shape* start_shape = core_->geomRegistry().getVertex(selected->ids[0]);
        const TopoDS_Shape* end_shape = core_->geomRegistry().getVertex(selected->ids[1]);
        if (!start_shape || !end_shape
            || start_shape->ShapeType() != TopAbs_VERTEX
            || end_shape->ShapeType() != TopAbs_VERTEX)
            throw std::invalid_argument("Selected geometry vertices are no longer valid");

        // 在索引释放前复制 TopoDS_Vertex，随后用它们构造共享端点的 Edge。
        const TopoDS_Vertex start = TopoDS::Vertex(*start_shape);
        const TopoDS_Vertex end = TopoDS::Vertex(*end_shape);
        TopoDS_Shape line = GeometryBuilder::makeLine(start, end);
        return addGeometryShape(
            -1, componentId, "Line_" + std::to_string(next_line_number_), std::move(line));
    } catch (const Standard_Failure& error) {
        const char* detail = error.GetMessageString();
        spdlog::error("QModelManager::createLineFromVertices: {}",
            detail ? detail : "OpenCASCADE error");
        emit geometryOperationFailed(
            QStringLiteral("创建 Line 失败：%1")
                .arg(QString::fromLocal8Bit(detail ? detail : "OpenCASCADE error")));
    } catch (const std::exception& error) {
        spdlog::error("QModelManager::createLineFromVertices: {}", error.what());
        emit geometryOperationFailed(
            QStringLiteral("创建 Line 失败：%1").arg(QString::fromLocal8Bit(error.what())));
    }
    return -1;
}

int QModelManager::createBox(
    int modelId,
    int componentId,
    double originX,
    double originY,
    double originZ,
    double lengthX,
    double lengthY,
    double lengthZ)
{
    try {
        TopoDS_Shape shape = GeometryBuilder::makeBox(
            originX, originY, originZ, lengthX, lengthY, lengthZ);
        const std::string component_name = "Box_" + std::to_string(next_box_number_);
        const Index result_component_id = addGeometryShape(
            modelId, componentId, component_name, std::move(shape));
        if (componentId < 0)
            ++next_box_number_;
        return result_component_id;
    } catch (const Standard_Failure& error) {
        const char* detail = error.GetMessageString();
        spdlog::error("QModelManager::createBox: {}", detail ? detail : "OpenCASCADE error");
        emit geometryOperationFailed(
            QStringLiteral("创建 Box 失败：%1").arg(QString::fromLocal8Bit(detail ? detail : "OpenCASCADE error")));
    } catch (const std::exception& error) {
        spdlog::error("QModelManager::createBox: {}", error.what());
        emit geometryOperationFailed(
            QStringLiteral("创建 Box 失败：%1").arg(QString::fromLocal8Bit(error.what())));
    }
    return -1;
}

Index QModelManager::addGeometryShape(
    Index model_id,
    Index component_id,
    std::string component_name,
    TopoDS_Shape shape)
{
    // Component 目标优先：将新形状追加到其现有 Geometry。
    if (component_id >= 0)
        return core_->appendGeometryShape(component_id, std::move(shape));

    const std::string model_name = "temp_" + component_name;

    auto geometry = std::make_unique<GeometryData>();
    geometry->rootShape = std::make_unique<TopoDS_Shape>(std::move(shape));

    auto component = std::make_unique<ComponentData>();
    component->name = std::move(component_name);
    component->geometry = std::move(geometry);

    if (model_id >= 0) {
        if (!core_->modelById(model_id))
            throw std::invalid_argument("Target model does not exist");
        return core_->addGeometryComponent(model_id, std::move(component));
    }

    ComponentDatas components;
    components.push_back(std::move(component));
    const Index new_model_id = core_->addModel(model_name, std::move(components));
    ModelData* model = core_->modelById(new_model_id);
    if (!model || model->componentIds().empty())
        throw std::runtime_error("Failed to add the geometry component");
    return model->componentIds().back();
}

QObject* QModelManager::getOperator(int id)
{
    auto maybeOp = core_->getModelOperator(id);
    if (!maybeOp)
        return nullptr;

    // 暂时不做包装器，直接返回 nullptr
    return nullptr;
    // 如果以后要 QML 操作 ModelOperator，请启用下面这行并实现包装器：
    // return new QModelOperatorWrapper(std::move(*maybeOp), this);
}

ModelLayer* QModelManager::getModelManager()
{
    return core_.get();
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

systems::QSystemPluginManager* QModelManager::getSystemPluginManager() const
{
    return q_plugin_manager_.get();
}

std::string_view QModelManager::argv0 = "./PreCess.exe";

QModelManager* QModelManager::create(QQmlEngine*, QJSEngine*)
{
    return new QModelManager(argv0);
}
