#include "ComponentData.h"
#include "EventBus.h"
#include "FeatureSystem.h"
#include "GeometryBuilder.h"
#include "GeometryData.h"
#include "ModelLayer.h"
#include "ModelOperator.h"
#include "Selection.h"
#include "SplitFaceHandler.h"
#include "UndoStack.h"

#include <BRep_Tool.hxx>
#include <BRep_Builder.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <NCollection_IndexedMap.hxx>
#include <catch2/catch_test_macros.hpp>
#include <gp_Pnt.hxx>

#include <any>
#include <memory>
#include <string>
#include <utility>

using namespace systems;
using namespace systems::feature;

namespace {
/**
 * @brief 分割面插件测试数据，保存组件与初始几何的业务 ID。
 */
struct SplitFaceFixture {
    Index component_id { -1 };
    GeomFaceId face_id { kInvalidGeomFaceId };
    GeomEdgeId splitting_edge_id { kInvalidGeomEdgeId };
};

/**
 * @brief 面切面测试数据，保存目标面、切割面及其组件 ID。
 */
struct SplitFaceByFaceFixture {
    Index component_id { -1 };
    GeomFaceId target_face_id { kInvalidGeomFaceId };
    GeomFaceId tool_face_id { kInvalidGeomFaceId };
};

HandlerMetaData handlerMetaData()
{
    HandlerMetaData meta_data;
    meta_data.name = "SplitFace";
    meta_data.display_name = "分割面";
    return meta_data;
}

/**
 * @brief 按坐标查找 Face 上已有顶点，使测试分割边与面边界共享拓扑点。
 */
TopoDS_Vertex findFaceVertex(const TopoDS_Face& face, double x, double y, double z)
{
    constexpr double tolerance = 1.0e-7;
    const gp_Pnt expected(x, y, z);
    for (TopExp_Explorer vertex_exp(face, TopAbs_VERTEX); vertex_exp.More(); vertex_exp.Next()) {
        const TopoDS_Vertex vertex = TopoDS::Vertex(vertex_exp.Current());
        if (BRep_Tool::Pnt(vertex).Distance(expected) <= tolerance)
            return vertex;
    }
    return {};
}

/**
 * @brief 创建一个矩形面和共享对角顶点的独立分割边，放入同一几何组件。
 */
SplitFaceFixture addSplittableFace(ModelLayer& model_layer, double offset_x = 0.0)
{
    const TopoDS_Face face = TopoDS::Face(
        GeometryBuilder::makeRectangleFace(
            offset_x, 0.0, 0.0, 10.0, 20.0, CoordinatePlane::XY));
    const TopoDS_Vertex first = findFaceVertex(face, offset_x, 0.0, 0.0);
    const TopoDS_Vertex second = findFaceVertex(face, offset_x + 10.0, 20.0, 0.0);
    REQUIRE_FALSE(first.IsNull());
    REQUIRE_FALSE(second.IsNull());
    const TopoDS_Edge diagonal = TopoDS::Edge(GeometryBuilder::makeLine(first, second));

    BRep_Builder builder;
    TopoDS_Compound shapes;
    builder.MakeCompound(shapes);
    builder.Add(shapes, face);
    builder.Add(shapes, diagonal);

    auto geometry = std::make_unique<GeometryData>();
    geometry->setRootShape(shapes);
    auto component = std::make_unique<ComponentData>();
    component->name = "SplittableFace";
    component->geometry = std::move(geometry);

    const Index model_id = model_layer.addModel("split_face_test", {});
    auto model_operator = model_layer.getModelOperator(model_id);
    REQUIRE(model_operator.has_value());
    const Index component_id = model_operator->addGeometryComponent(std::move(component));

    ComponentData* stored = model_layer.findComponent(component_id);
    REQUIRE(stored != nullptr);
    stored->geometry->ensureIndexBuilt(model_layer.geomRegistry());
    const auto& index = stored->geometry->index;
    const int face_local = index.type_maps[index.typeIndex(TopAbs_FACE)].FindIndex(face);
    const int edge_local = index.type_maps[index.typeIndex(TopAbs_EDGE)].FindIndex(diagonal);
    REQUIRE(face_local > 0);
    REQUIRE(edge_local > 0);

    return {
        component_id,
        index.faceGlobalId(face_local),
        index.edgeGlobalId(edge_local),
    };
}

/**
 * @brief 创建两个正交相交的矩形面，用于验证面切面分割流程。
 */
SplitFaceByFaceFixture addFaceCutter(ModelLayer& model_layer)
{
    const TopoDS_Face target = TopoDS::Face(
        GeometryBuilder::makeRectangleFace(
            0.0, 0.0, 0.0, 10.0, 10.0, CoordinatePlane::XY));
    const TopoDS_Face tool = TopoDS::Face(
        GeometryBuilder::makeRectangleFace(
            0.0, 5.0, -5.0, 10.0, 10.0, CoordinatePlane::XZ));

    BRep_Builder builder;
    TopoDS_Compound shapes;
    builder.MakeCompound(shapes);
    builder.Add(shapes, target);
    builder.Add(shapes, tool);

    auto geometry = std::make_unique<GeometryData>();
    geometry->setRootShape(shapes);
    auto component = std::make_unique<ComponentData>();
    component->name = "FaceCutter";
    component->geometry = std::move(geometry);

    const Index model_id = model_layer.addModel("split_face_by_face_test", {});
    auto model_operator = model_layer.getModelOperator(model_id);
    REQUIRE(model_operator.has_value());
    const Index component_id = model_operator->addGeometryComponent(std::move(component));

    ComponentData* stored = model_layer.findComponent(component_id);
    REQUIRE(stored != nullptr);
    stored->geometry->ensureIndexBuilt(model_layer.geomRegistry());
    const auto& index = stored->geometry->index;
    const int target_local = index.type_maps[index.typeIndex(TopAbs_FACE)].FindIndex(target);
    const int tool_local = index.type_maps[index.typeIndex(TopAbs_FACE)].FindIndex(tool);
    REQUIRE(target_local > 0);
    REQUIRE(tool_local > 0);
    return {
        component_id,
        index.faceGlobalId(target_local),
        index.faceGlobalId(tool_local),
    };
}

std::shared_ptr<Selection> makeSelection(
    ElementEnum::Type type,
    Index component_id,
    Index shape_id)
{
    auto selection = std::make_shared<Selection>();
    selection->type = type;
    selection->component_id = component_id;
    selection->ids = { shape_id };
    return selection;
}

int countFaces(const ComponentData& component)
{
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> faces;
    TopExp::MapShapes(*component.geometry->rootShape, TopAbs_FACE, faces);
    return faces.Extent();
}
}

TEST_CASE("SplitFace feature splits one face and records one undo operation", "[SplitFacePlugin]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    UndoStack undo_stack(model_layer);
    model_layer.setUndoRecorder(&undo_stack);
    FeatureSystem feature_system(model_layer, bus, &undo_stack);

    const SplitFaceFixture fixture = addSplittableFace(model_layer);
    undo_stack.clear();
    FeatureSystem::SystemHandlerPtr handler { new SplitFaceHandler };
    REQUIRE(feature_system.registerHandler(handlerMetaData(), std::move(handler)));

    const std::any hint = feature_system.invoke("SplitFace");
    REQUIRE(std::any_cast<std::string>(hint) == "请选择一个需要分割的几何面。");

    REQUIRE(feature_system.setParameter("SplitFace", 0,
        core::ArgObject::create<ArgTypeEnum::Selector>(makeSelection(
            ElementEnum::GeometryFace, fixture.component_id, fixture.face_id))));
    REQUIRE(feature_system.setParameter("SplitFace", 1,
        core::ArgObject::create<ArgTypeEnum::Selector>(makeSelection(
            ElementEnum::GeometryEdge, fixture.component_id, fixture.splitting_edge_id))));

    const std::string result = std::any_cast<std::string>(feature_system.invoke("SplitFace"));
    REQUIRE(result.find("成功") != std::string::npos);
    REQUIRE(countFaces(*model_layer.findComponent(fixture.component_id)) == 2);
    REQUIRE(undo_stack.undoLabel() == "分割面");

    undo_stack.undo();
    REQUIRE(countFaces(*model_layer.findComponent(fixture.component_id)) == 1);
    undo_stack.redo();
    REQUIRE(countFaces(*model_layer.findComponent(fixture.component_id)) == 2);
}

TEST_CASE("SplitFace feature rejects edges from another component", "[SplitFacePlugin]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem feature_system(model_layer, bus);
    const SplitFaceFixture target = addSplittableFace(model_layer);
    const SplitFaceFixture other = addSplittableFace(model_layer, 30.0);

    FeatureSystem::SystemHandlerPtr handler { new SplitFaceHandler };
    REQUIRE(feature_system.registerHandler(handlerMetaData(), std::move(handler)));
    REQUIRE(feature_system.setParameter("SplitFace", 0,
        core::ArgObject::create<ArgTypeEnum::Selector>(makeSelection(
            ElementEnum::GeometryFace, target.component_id, target.face_id))));
    REQUIRE(feature_system.setParameter("SplitFace", 1,
        core::ArgObject::create<ArgTypeEnum::Selector>(makeSelection(
            ElementEnum::GeometryEdge, other.component_id, other.splitting_edge_id))));

    const std::any result = feature_system.invoke("SplitFace");
    REQUIRE(std::any_cast<std::string>(result)
        == "目标面和切割工具必须属于同一个组件。");
    REQUIRE(countFaces(*model_layer.findComponent(target.component_id)) == 1);
}

TEST_CASE("SplitFace feature splits a face with an intersecting face", "[SplitFacePlugin]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem feature_system(model_layer, bus);
    const SplitFaceByFaceFixture fixture = addFaceCutter(model_layer);

    FeatureSystem::SystemHandlerPtr handler { new SplitFaceHandler };
    REQUIRE(feature_system.registerHandler(handlerMetaData(), std::move(handler)));
    REQUIRE(feature_system.setParameter("SplitFace", 0,
        core::ArgObject::create<ArgTypeEnum::Selector>(makeSelection(
            ElementEnum::GeometryFace, fixture.component_id, fixture.target_face_id))));
    REQUIRE(feature_system.setParameter("SplitFace", 1,
        core::ArgObject::create<ArgTypeEnum::Selector>(makeSelection(
            ElementEnum::GeometryFace, fixture.component_id, fixture.tool_face_id))));

    const std::string result = std::any_cast<std::string>(feature_system.invoke("SplitFace"));
    REQUIRE(result.find("成功") != std::string::npos);
    REQUIRE(countFaces(*model_layer.findComponent(fixture.component_id)) == 3);
}

TEST_CASE("SplitFace feature rejects a component with geometry mesh mapping", "[SplitFacePlugin]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem feature_system(model_layer, bus);
    const SplitFaceFixture fixture = addSplittableFace(model_layer);

    ComponentData* component = model_layer.findComponent(fixture.component_id);
    component->mapping = std::make_unique<GeometryMeshMap>();
    component->mapping->geometry_edge_to_mesh_point_ids[fixture.splitting_edge_id] = { 0, 1 };

    FeatureSystem::SystemHandlerPtr handler { new SplitFaceHandler };
    REQUIRE(feature_system.registerHandler(handlerMetaData(), std::move(handler)));
    REQUIRE(feature_system.setParameter("SplitFace", 0,
        core::ArgObject::create<ArgTypeEnum::Selector>(makeSelection(
            ElementEnum::GeometryFace, fixture.component_id, fixture.face_id))));
    REQUIRE(feature_system.setParameter("SplitFace", 1,
        core::ArgObject::create<ArgTypeEnum::Selector>(makeSelection(
            ElementEnum::GeometryEdge, fixture.component_id, fixture.splitting_edge_id))));

    const std::any result = feature_system.invoke("SplitFace");
    REQUIRE(std::any_cast<std::string>(result)
        == "目标组件已经建立几何-网格映射，不能修改几何拓扑。");
    REQUIRE(countFaces(*component) == 1);
}
