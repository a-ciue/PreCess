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
 * @brief 补间隙插件测试数据，保存组件与种子自由边业务 ID。
 */
struct FillGapFixture {
    Index component_id { -1 };
    Index seed_edge_id { -1 };
};

HandlerMetaData handlerMetaData()
{
    HandlerMetaData meta_data;
    meta_data.name = "FillGap";
    meta_data.display_name = "补间隙";
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
    return {};
}

FillGapFixture addGapPair(ModelLayer& model_layer)
{
    const TopoDS_Face left = TopoDS::Face(GeometryBuilder::makeRectangleFace(
        0.0, 0.0, 0.0, 10.0, 10.0, CoordinatePlane::XY));
    const TopoDS_Face right = TopoDS::Face(GeometryBuilder::makeRectangleFace(
        10.005, 0.0, 0.0, 10.0, 10.0, CoordinatePlane::XY));
    const TopoDS_Edge seed = findEdgeOnX(left, 10.0);
    REQUIRE_FALSE(seed.IsNull());

    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    builder.Add(compound, left);
    builder.Add(compound, right);

    auto geometry = std::make_unique<GeometryData>();
    geometry->setRootShape(compound);
    auto component = std::make_unique<ComponentData>();
    component->name = "GapPair";
    component->geometry = std::move(geometry);

    const Index model_id = model_layer.addModel("fill_gap_test", {});
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

    const Index result = std::any_cast<Index>(feature_system.invoke("FillGap"));
    REQUIRE(result == fixture.component_id);
    REQUIRE(countSharedEdges(*model_layer.findComponent(fixture.component_id)) == 1);
    REQUIRE(undo_stack.undoLabel() == "补间隙");

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
    REQUIRE(std::any_cast<std::string>(hint).find("补间隙失败") != std::string::npos);
    REQUIRE(countSharedEdges(*model_layer.findComponent(fixture.component_id)) == 0);
}
