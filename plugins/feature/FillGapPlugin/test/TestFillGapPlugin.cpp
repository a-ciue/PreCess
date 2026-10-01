#include "ComponentData.h"
#include "EventBus.h"
#include "FeatureSystem.h"
#include "FillGapHandler.h"
#include "GeometryBuilder.h"
#include "GeometryData.h"
#include "ModelLayer.h"
#include "ModelOperator.h"
#include "Selection.h"
#include "UndoStack.h"

#include <BRep_Builder.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRep_Tool.hxx>
#include <NCollection_IndexedDataMap.hxx>
#include <NCollection_IndexedMap.hxx>
#include <NCollection_List.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <catch2/catch_test_macros.hpp>

#include <any>
#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace systems;
using namespace systems::feature;

namespace {
/**
 * @brief 局部缝合插件测试数据，保存组件与种子自由边业务 ID。
 */
struct FillGapFixture {
    Index component_id { -1 };
    Index seed_edge_id { -1 };
};

HandlerMetaData handlerMetaData()
{
    HandlerMetaData meta_data;
    meta_data.name = "FillGap";
    meta_data.display_name = "局部缝合";
    return meta_data;
}

TopoDS_Edge findEdgeOnX(const TopoDS_Face& face, double x)
{
    constexpr double tolerance = 1.0e-7;
    for (TopExp_Explorer edge_exp(face, TopAbs_EDGE); edge_exp.More(); edge_exp.Next()) {
        const TopoDS_Edge edge = TopoDS::Edge(edge_exp.Current());
        TopoDS_Vertex start;
        TopoDS_Vertex end;
        TopExp::Vertices(edge, start, end, true);
        if (!start.IsNull() && !end.IsNull()
            && std::abs(BRep_Tool::Pnt(start).X() - x) <= tolerance
            && std::abs(BRep_Tool::Pnt(end).X() - x) <= tolerance)
            return edge;
    }
    return { };
}

FillGapFixture addGapPair(ModelLayer& model_layer, bool reverse_retry = false)
{
    const TopoDS_Face left = TopoDS::Face(GeometryBuilder::makeRectangleFace(
        0.0, 0.0, 0.0, 10.0, 10.0, CoordinatePlane::XY));
    TopoDS_Face right;
    TopoDS_Vertex middle;
    if (reverse_retry) {
        // 右侧分段边的中间顶点带支路，正向整链替换会拒绝，反向保留它则可成功。
        BRepBuilderAPI_MakePolygon polygon;
        for (const auto& point : { gp_Pnt(10.005, 0, 0), gp_Pnt(20.005, 0, 0), gp_Pnt(20.005, 10, 0), gp_Pnt(10.005, 10, 0), gp_Pnt(10.005, 5, 0) })
            polygon.Add(point);
        polygon.Close();
        right = BRepBuilderAPI_MakeFace(polygon.Wire()).Face();
        for (TopExp_Explorer vertex(right, TopAbs_VERTEX); vertex.More(); vertex.Next()) {
            const auto candidate = TopoDS::Vertex(vertex.Current());
            if (BRep_Tool::Pnt(candidate).Distance(gp_Pnt(10.005, 5, 0)) < 1.e-7)
                middle = candidate;
        }
        REQUIRE_FALSE(middle.IsNull());
    } else {
        right = TopoDS::Face(GeometryBuilder::makeRectangleFace(10.005, 0, 0, 10, 10, CoordinatePlane::XY));
    }
    const TopoDS_Edge seed = reverse_retry ? findEdgeOnX(right, 10.005) : findEdgeOnX(left, 10.0);
    REQUIRE_FALSE(seed.IsNull());

    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    builder.Add(compound, left);
    builder.Add(compound, right);
    if (reverse_retry) {
        const auto tip = TopoDS::Vertex(GeometryBuilder::makePoint(10.005, 5, 2));
        builder.Add(compound, BRepBuilderAPI_MakeEdge(middle, tip).Edge());
    }

    auto geometry = std::make_unique<GeometryData>();
    geometry->setRootShape(compound);
    auto component = std::make_unique<ComponentData>();
    component->name = "GapPair";
    component->geometry = std::move(geometry);

    const Index model_id = model_layer.addModel("fill_gap_test", { });
    auto model_operator = model_layer.getModelOperator(model_id);
    REQUIRE(model_operator.has_value());
    const Index component_id = model_operator->addGeometryComponent(std::move(component));

    ComponentData* stored = model_layer.findComponent(component_id);
    REQUIRE(stored != nullptr);
    stored->geometry->ensureIndexBuilt(model_layer.geomRegistry());
    const auto& index = stored->geometry->index;
    const int seed_local = index.type_maps[index.typeIndex(TopAbs_EDGE)].FindIndex(seed);
    REQUIRE(seed_local > 0);
    return { component_id, index.edgeGlobalId(seed_local) };
}

std::shared_ptr<Selection> makeEdgeSelection(const FillGapFixture& fixture)
{
    auto selection = std::make_shared<Selection>();
    selection->type = ElementEnum::GeometryEdge;
    selection->component_id = fixture.component_id;
    selection->ids = { fixture.seed_edge_id };
    return selection;
}

int countSharedEdges(const ComponentData& component)
{
    NCollection_IndexedDataMap<TopoDS_Shape, NCollection_List<TopoDS_Shape>,
        TopTools_ShapeMapHasher>
        edge_faces;
    TopExp::MapShapesAndUniqueAncestors(
        *component.geometry->rootShape, TopAbs_EDGE, TopAbs_FACE, edge_faces);
    int count = 0;
    for (int index = 1; index <= edge_faces.Extent(); ++index) {
        const NCollection_List<TopoDS_Shape>& faces = edge_faces.FindFromIndex(index);
        NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> unique_faces;
        for (NCollection_List<TopoDS_Shape>::Iterator it(faces); it.More(); it.Next())
            unique_faces.Add(it.Value());
        if (unique_faces.Extent() > 1)
            ++count;
    }
    return count;
}
}

TEST_CASE("FillGap feature stitches the gap boundary from one seed edge", "[FillGapPlugin]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    UndoStack undo_stack(model_layer);
    model_layer.setUndoRecorder(&undo_stack);
    FeatureSystem feature_system(model_layer, bus, &undo_stack);

    const FillGapFixture fixture = addGapPair(model_layer);
    undo_stack.clear();
    FeatureSystem::SystemHandlerPtr handler { new FillGapHandler };
    REQUIRE(feature_system.registerHandler(handlerMetaData(), std::move(handler)));

    REQUIRE(feature_system.setParameter("FillGap", 0,
        core::ArgObject::create<ArgTypeEnum::Selector>(makeEdgeSelection(fixture))));
    REQUIRE(feature_system.setParameter("FillGap", 1,
        core::ArgObject::create<ArgTypeEnum::Float>(0.01)));

    const std::string result = std::any_cast<std::string>(feature_system.invoke("FillGap"));
    REQUIRE(result.find("成功") != std::string::npos);
    REQUIRE(countSharedEdges(*model_layer.findComponent(fixture.component_id)) == 1);
    REQUIRE(undo_stack.undoLabel() == "局部缝合");

    undo_stack.undo();
    REQUIRE(countSharedEdges(*model_layer.findComponent(fixture.component_id)) == 0);
}

TEST_CASE("FillGap feature reports missing partner when tolerance is too small", "[FillGapPlugin]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    UndoStack undo_stack(model_layer);
    model_layer.setUndoRecorder(&undo_stack);
    FeatureSystem feature_system(model_layer, bus, &undo_stack);

    const FillGapFixture fixture = addGapPair(model_layer);
    FeatureSystem::SystemHandlerPtr handler { new FillGapHandler };
    REQUIRE(feature_system.registerHandler(handlerMetaData(), std::move(handler)));

    REQUIRE(feature_system.setParameter("FillGap", 0,
        core::ArgObject::create<ArgTypeEnum::Selector>(makeEdgeSelection(fixture))));
    REQUIRE(feature_system.setParameter("FillGap", 1,
        core::ArgObject::create<ArgTypeEnum::Float>(0.001)));

    const std::any hint = feature_system.invoke("FillGap");
    REQUIRE(std::any_cast<std::string>(hint).find("局部缝合失败") != std::string::npos);
    REQUIRE(countSharedEdges(*model_layer.findComponent(fixture.component_id)) == 0);
}

//! @brief 反向成功显示方向提示，两次几何尝试仅产生一次可撤销的模型写入。
TEST_CASE("FillGap feature reports reverse success as one undo operation", "[FillGapPlugin]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    UndoStack undo_stack(model_layer);
    model_layer.setUndoRecorder(&undo_stack);
    FeatureSystem feature_system(model_layer, bus, &undo_stack);
    const auto fixture = addGapPair(model_layer, true);
    undo_stack.clear();
    FeatureSystem::SystemHandlerPtr handler { new FillGapHandler };
    REQUIRE(feature_system.registerHandler(handlerMetaData(), std::move(handler)));
    REQUIRE(feature_system.setParameter("FillGap", 0, core::ArgObject::create<ArgTypeEnum::Selector>(makeEdgeSelection(fixture))));
    const auto result = feature_system.invoke("FillGap");
    REQUIRE(std::any_cast<std::string>(result).find("已反向缝合") != std::string::npos);
    REQUIRE(countSharedEdges(*model_layer.findComponent(fixture.component_id)) == 2);
    undo_stack.undo();
    REQUIRE(countSharedEdges(*model_layer.findComponent(fixture.component_id)) == 0);
    REQUIRE_FALSE(undo_stack.canUndo());
    undo_stack.redo();
    REQUIRE(countSharedEdges(*model_layer.findComponent(fixture.component_id)) == 2);
}
