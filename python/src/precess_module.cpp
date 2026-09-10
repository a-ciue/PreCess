/**
 * @file precess_module.cpp
 * @brief precess Python 绑定模块：会话层（Session/SessionQuery/UndoStack）与功能调用
 *
 * 只绑定会话层稳定接口：脚本经 query 做只读查询、经结构操作入口与
 * FeatureSystem invoke 修改模型（invoke 为操作边界，undo/flush 由框架处理），
 * 不直接暴露 ModelLayer 写路径。渲染数据视图（MeshDataVtk/GeometryDataVtk）
 * 与 Qt 相关设施不在绑定范围内。
 */
#include "Session.h"

#include "FeatureInfo.h"
#include "FeatureParams.h"
#include "FeatureSystem.h"
#include "SessionQuery.h"
#include "UndoStack.h"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <any>
#include <filesystem>
#include <typeinfo>

namespace py = pybind11;
using namespace session;

namespace {

// 功能执行结果的类型擦除值 -> Python 对象（覆盖 FeatureHandler 的常用返回类型）
py::object anyToPython(const std::any& value)
{
    if (!value.has_value())
        return py::none();
    if (value.type() == typeid(std::string))
        return py::cast(std::any_cast<std::string>(value));
    if (value.type() == typeid(long long))
        return py::cast(std::any_cast<long long>(value));
    if (value.type() == typeid(int))
        return py::cast(std::any_cast<int>(value));
    if (value.type() == typeid(double))
        return py::cast(std::any_cast<double>(value));
    if (value.type() == typeid(bool))
        return py::cast(std::any_cast<bool>(value));
    if (value.type() == typeid(std::vector<double>))
        return py::cast(std::any_cast<const std::vector<double>&>(value));
    return py::none();
}

// 按参数声明类型把 Python 值转换为 ArgObject（与 QFeatureSystemAdaptor::setParameter 同约定）
core::ArgObject pythonToArgObject(const core::ArgType& type, py::handle value)
{
    switch (type.type) {
    case ArgTypeEnum::Int:
        return core::ArgObject::create<ArgTypeEnum::Int>(value.cast<long long>());
    case ArgTypeEnum::Float:
        return core::ArgObject::create<ArgTypeEnum::Float>(value.cast<double>());
    case ArgTypeEnum::Text:
        return core::ArgObject::create<ArgTypeEnum::Text>(value.cast<std::string>());
    case ArgTypeEnum::Bool:
        return core::ArgObject::create<ArgTypeEnum::Bool>(value.cast<bool>());
    case ArgTypeEnum::Path:
        return core::ArgObject::create<ArgTypeEnum::Path>(std::filesystem::path(value.cast<std::string>()));
    case ArgTypeEnum::Combo:
        return core::ArgObject::create<ArgTypeEnum::Combo>(value.cast<int>());
    default:
        throw py::type_error("参数 \"" + type.name + "\" 的类型暂不支持从 Python 设置");
    }
}

} // namespace

PYBIND11_MODULE(precess, m)
{
    m.doc() = "PreCess 会话层绑定：模型查询、结构操作、undo 与功能调用（无 Qt 依赖）";

    // —— 查询结果结构体（只读视图）——
    py::class_<ModelSummary>(m, "ModelSummary")
        .def_readonly("model_id", &ModelSummary::model_id)
        .def_readonly("name", &ModelSummary::name)
        .def_readonly("component_count", &ModelSummary::component_count)
        .def("__repr__", [](const ModelSummary& s) {
            return "ModelSummary(model_id=" + std::to_string(s.model_id) + ", name='" + s.name + "')";
        });

    py::class_<ComponentSummary>(m, "ComponentSummary")
        .def_readonly("component_id", &ComponentSummary::component_id)
        .def_readonly("name", &ComponentSummary::name)
        .def_readonly("has_mesh", &ComponentSummary::has_mesh)
        .def_readonly("has_geometry", &ComponentSummary::has_geometry)
        .def_readonly("material_id", &ComponentSummary::material_id);

    py::class_<MeshSummary>(m, "MeshSummary")
        .def_readonly("has_mesh", &MeshSummary::has_mesh)
        .def_readonly("vertex_count", &MeshSummary::vertex_count)
        .def_readonly("edge_count", &MeshSummary::edge_count)
        .def_readonly("face_count", &MeshSummary::face_count)
        .def_readonly("solid_count", &MeshSummary::solid_count);

    py::class_<GeometrySummary>(m, "GeometrySummary")
        .def_readonly("has_geometry", &GeometrySummary::has_geometry)
        .def_readonly("vertex_count", &GeometrySummary::vertex_count)
        .def_readonly("edge_count", &GeometrySummary::edge_count)
        .def_readonly("face_count", &GeometrySummary::face_count)
        .def_readonly("solid_count", &GeometrySummary::solid_count);

    py::class_<AttributeInfo>(m, "AttributeInfo")
        .def_readonly("name", &AttributeInfo::name)
        .def_readonly("display_name", &AttributeInfo::display_name)
        .def_readonly("type", &AttributeInfo::type)
        .def_readonly("type_name", &AttributeInfo::type_name)
        .def_readonly("attr_type", &AttributeInfo::attr_type)
        .def_readonly("component_count", &AttributeInfo::component_count);

    // —— 只读查询 ——
    py::class_<SessionQuery>(m, "SessionQuery")
        .def("list_models", &SessionQuery::listModels)
        .def("component_summaries", &SessionQuery::componentSummaries)
        .def("mesh_summary", &SessionQuery::meshSummary)
        .def("geometry_summary", &SessionQuery::geometrySummary)
        .def("component_attribute_infos", &SessionQuery::componentAttributeInfos)
        .def("component_ids", &SessionQuery::componentIds)
        .def("first_mesh_component_id", &SessionQuery::firstMeshComponentId)
        .def("find_model_id_by_component", &SessionQuery::findModelIdByComponent)
        .def("has_model", &SessionQuery::hasModel)
        .def("has_component", &SessionQuery::hasComponent)
        .def("model_name", &SessionQuery::modelName)
        .def("component_name", &SessionQuery::componentName)
        .def("point_global_id", &SessionQuery::pointGlobalId)
        .def("find_edge_by_endpoints", &SessionQuery::findEdgeByEndpoints)
        .def("geometry_edge_mapped_point_ids", &SessionQuery::geometryEdgeMappedPointIds)
        .def("resolve_geometry_face_local_id", &SessionQuery::resolveGeometryFaceLocalId)
        .def("resolve_geometry_edge_local_id", &SessionQuery::resolveGeometryEdgeLocalId)
        .def("resolve_geometry_vertex_local_id", &SessionQuery::resolveGeometryVertexLocalId)
        .def("resolve_geometry_solid_local_id", &SessionQuery::resolveGeometrySolidLocalId);

    // —— undo 栈 ——
    py::class_<UndoStack>(m, "UndoStack")
        .def("can_undo", &UndoStack::canUndo)
        .def("can_redo", &UndoStack::canRedo)
        .def("undo_label", [](const UndoStack& s) { return s.undoLabel().value_or(std::string()); })
        .def("redo_label", [](const UndoStack& s) { return s.redoLabel().value_or(std::string()); })
        .def("undo", &UndoStack::undo)
        .def("redo", &UndoStack::redo)
        .def("staged_active", &UndoStack::stagedActive);

    // —— 会话 ——
    py::class_<Session>(m, "Session")
        .def(py::init<>())
        .def_property_readonly("query", &Session::query, py::return_value_policy::reference_internal)
        .def_property_readonly("undo_stack", &Session::undoStack, py::return_value_policy::reference_internal)
        .def("load_static_plugins", &Session::loadStaticPlugins)
        .def("load_plugins_from_directory", [](Session& s, const std::string& plugin_dir) { s.loadPluginsFromDirectory(std::filesystem::path(plugin_dir)); }, py::arg("plugin_dir"), "扫描目录装载动态插件（str，UTF-8）")
        .def("remove_model", &Session::removeModel)
        .def("remove_component", &Session::removeComponent)
        .def("remove_mesh", &Session::removeMesh)
        .def("remove_geometry", &Session::removeGeometry)
        .def("teardown", &Session::teardown)
        // 功能调用：invoke 是操作边界（undo 自动记录 + 通知 flush）
        .def("feature_names", [](Session& s) {
            std::vector<std::string> names;
            for (const systems::feature::FeatureInfo* info : s.featureSystem().getFeatureInfos())
                names.push_back(info->name);
            return names; })
        .def("invoke", [](Session& s, const std::string& unique_name) { return anyToPython(s.featureSystem().invoke(unique_name)); })
        .def("set_parameter", [](Session& s, const std::string& unique_name, std::size_t index, py::handle value) -> bool {
            const systems::feature::FeatureParams* params = s.featureSystem().params(unique_name);
            if (!params || index >= params->count())
                throw py::value_error("功能 \"" + unique_name + "\" 不存在参数下标 " + std::to_string(index));
            return s.featureSystem().setParameter(unique_name, index, pythonToArgObject(params->types()[index], value)); });
}
