#include "ComponentData.h"
#include "EventBus.h"
#include "FeatureSystem.h"
#include "PatchFaceHandler.h"
#include "GeometryBuilder.h"
#include "GeometryData.h"
#include "GeometryTopologyEditor.h"
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
 * @brief 补面插件测试数据，保存组件与种子自由边业务 ID。
 */
struct PatchFaceFixture {
    Index component_id { -1 };
    Index seed_edge_id { -1 };
};

//! @brief 注册独立补面功能，验证菜单标签与撤销标签。
HandlerMetaData handlerMetaData()
{
    HandlerMetaData meta_data;
    meta_data.name = "PatchFace";
    meta_data.display_name = "补面";
    return meta_data;
}

//! @brief 构造盒子孔洞或非级联删除面留下的孤立边环，并取得种子边业务 ID。
PatchFaceFixture addPatchBoundary(ModelLayer& model_layer, bool isolated)
{
    const auto shape = isolated
        ? GeometryBuilder::makeRectangleFace(0, 0, 0, 10, 10, CoordinatePlane::XY)
        : GeometryBuilder::makeBox(0, 0, 0, 10, 10, 10);
    const auto cap = isolated ? TopoDS::Face(shape) : TopoDS::Face(TopExp_Explorer(shape, TopAbs_FACE).Current());
    const auto seed = TopoDS::Edge(TopExp_Explorer(cap, TopAbs_EDGE).Current());
    const auto open = GeometryTopologyEditor::removeShape(shape, cap, !isolated);

    auto geometry = std::make_unique<GeometryData>();
    geometry->setRootShape(open);
    auto component = std::make_unique<ComponentData>();
    component->name = "OpenBox";
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

//! @brief 使用边选择参数定位组件，不依赖对象树选择。
std::shared_ptr<Selection> makeEdgeSelection(const PatchFaceFixture& fixture)
{
    auto selection = std::make_shared<Selection>();
    selection->type = ElementEnum::GeometryEdge;
    selection->component_id = fixture.component_id;
    selection->ids = { fixture.seed_edge_id };
    return selection;
}

//! @brief 统计补面前后的共享边数量，确认新面已连接。
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

//! @brief 独立补面按钮只需选择边界边，新增一个面且支持一次撤销/重做。
TEST_CASE("PatchFace feature fills a boundary loop with undo and redo", "[PatchFacePlugin]")
{
    for (bool isolated : { false, true }) {
        CAPTURE(isolated);
        core::EventBus bus;
        ModelLayer model_layer;
        const auto fixture = addPatchBoundary(model_layer, isolated);
        UndoStack undo_stack(model_layer);
        model_layer.setUndoRecorder(&undo_stack);
        FeatureSystem feature_system(model_layer, bus, &undo_stack);
        undo_stack.clear();
        FeatureSystem::SystemHandlerPtr handler { new PatchFaceHandler };
        REQUIRE(feature_system.registerHandler(handlerMetaData(), std::move(handler)));
        REQUIRE(feature_system.setParameter("PatchFace", 0,
            core::ArgObject::create<ArgTypeEnum::Selector>(makeEdgeSelection(fixture))));
        const auto result = feature_system.invoke("PatchFace");
        REQUIRE(std::any_cast<std::string>(result).find("成功") != std::string::npos);
        const auto* component = model_layer.findComponent(fixture.component_id);
        REQUIRE(component->geometry->index.face_local_to_global.size() == (isolated ? 2 : 7));
        REQUIRE(countSharedEdges(*component) == (isolated ? 0 : 12));
        REQUIRE(undo_stack.undoLabel() == "补面");
        undo_stack.undo();
        REQUIRE(component->geometry->index.face_local_to_global.size() == (isolated ? 1 : 6));
        REQUIRE(countSharedEdges(*component) == (isolated ? 0 : 8));
        undo_stack.redo();
        REQUIRE(component->geometry->index.face_local_to_global.size() == (isolated ? 2 : 7));
        REQUIRE(countSharedEdges(*component) == (isolated ? 0 : 12));
    }
}
