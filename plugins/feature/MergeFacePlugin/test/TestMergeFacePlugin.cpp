#include "ComponentData.h"
#include "EventBus.h"
#include "FeatureSystem.h"
#include "GeometryBuilder.h"
#include "GeometryData.h"
#include "GeometryTopologyEditor.h"
#include "MergeFaceHandler.h"
#include "ModelLayer.h"
#include "ModelOperator.h"
#include "Selection.h"
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
#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace systems;
using namespace systems::feature;

namespace {
/**
 * @brief 合并面插件测试数据，保存组件与两个待合并面的业务 ID。
 */
struct MergeFaceFixture {
    Index component_id { -1 };
    std::vector<GeomFaceId> face_ids;
};

HandlerMetaData handlerMetaData()
{
    HandlerMetaData meta_data;
    meta_data.name = "MergeFace";
    meta_data.display_name = "合并面";
    return meta_data;
}

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

std::vector<TopoDS_Face> findFacesOnZPlane(const TopoDS_Shape& shape, double z)
{
    constexpr double tolerance = 1.0e-7;
    std::vector<TopoDS_Face> faces;
    for (TopExp_Explorer face_exp(shape, TopAbs_FACE); face_exp.More(); face_exp.Next()) {
        const TopoDS_Face face = TopoDS::Face(face_exp.Current());
        bool on_plane = true;
        for (TopExp_Explorer vertex_exp(face, TopAbs_VERTEX); vertex_exp.More(); vertex_exp.Next()) {
            if (std::abs(BRep_Tool::Pnt(TopoDS::Vertex(vertex_exp.Current())).Z() - z) > tolerance) {
                on_plane = false;
                break;
            }
        }
        if (on_plane)
            faces.push_back(face);
    }
    return faces;
}

/**
 * @brief 先把长方体底面分成两个共面 Face，再加入模型作为合并输入。
 */
MergeFaceFixture addMergeableFaces(ModelLayer& model_layer)
{
    const TopoDS_Shape box =
        GeometryBuilder::makeBox(0.0, 0.0, 0.0, 10.0, 20.0, 30.0);
    const std::vector<TopoDS_Face> bottom_faces = findFacesOnZPlane(box, 0.0);
    REQUIRE(bottom_faces.size() == 1);
    const TopoDS_Face bottom_face = bottom_faces.front();
    const TopoDS_Vertex first = findFaceVertex(bottom_face, 0.0, 0.0, 0.0);
    const TopoDS_Vertex second = findFaceVertex(bottom_face, 10.0, 20.0, 0.0);
    REQUIRE_FALSE(first.IsNull());
    REQUIRE_FALSE(second.IsNull());
    const TopoDS_Edge diagonal = TopoDS::Edge(GeometryBuilder::makeLine(first, second));

    // 保留独立对角线，复现“创建线 -> 分割面 -> 合并面”的实际组件结构。
    BRep_Builder builder;
    TopoDS_Compound shapes;
    builder.MakeCompound(shapes);
    builder.Add(shapes, box);
    builder.Add(shapes, diagonal);
    GeometryData root_geometry;
    root_geometry.setRootShape(shapes);
    const TopoDS_Shape split_result = GeometryTopologyEditor::splitFace(
        *root_geometry.rootShape, bottom_face, { diagonal });
    const std::vector<TopoDS_Face> split_faces = findFacesOnZPlane(split_result, 0.0);
    REQUIRE(split_faces.size() == 2);

    auto geometry = std::make_unique<GeometryData>();
    geometry->setRootShape(split_result);
    auto component = std::make_unique<ComponentData>();
    component->name = "MergeableFaces";
    component->geometry = std::move(geometry);

    const Index model_id = model_layer.addModel("merge_face_test", {});
    auto model_operator = model_layer.getModelOperator(model_id);
    REQUIRE(model_operator.has_value());
    const Index component_id = model_operator->addGeometryComponent(std::move(component));

    ComponentData* stored = model_layer.findComponent(component_id);
    REQUIRE(stored != nullptr);
    stored->geometry->ensureIndexBuilt(model_layer.geomRegistry());
    const auto& index = stored->geometry->index;

    std::vector<GeomFaceId> face_ids;
    for (const TopoDS_Face& face : split_faces) {
        const int local_id = index.type_maps[index.typeIndex(TopAbs_FACE)].FindIndex(face);
        REQUIRE(local_id > 0);
        face_ids.push_back(index.faceGlobalId(local_id));
    }
    return { component_id, std::move(face_ids) };
}

std::shared_ptr<Selection> makeFaceSelection(const MergeFaceFixture& fixture)
{
    auto selection = std::make_shared<Selection>();
    selection->type = ElementEnum::GeometryFace;
    selection->component_id = fixture.component_id;
    selection->ids.assign(fixture.face_ids.begin(), fixture.face_ids.end());
    return selection;
}

int countFaces(const ComponentData& component)
{
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> faces;
    TopExp::MapShapes(*component.geometry->rootShape, TopAbs_FACE, faces);
    return faces.Extent();
}

int countEdges(const ComponentData& component)
{
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> edges;
    TopExp::MapShapes(*component.geometry->rootShape, TopAbs_EDGE, edges);
    return edges.Extent();
}

int countMergedBottomFaceEdges(const ComponentData& component)
{
    const std::vector<TopoDS_Face> faces =
        findFacesOnZPlane(*component.geometry->rootShape, 0.0);
    if (faces.size() != 1)
        return -1;

    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> edges;
    TopExp::MapShapes(faces.front(), TopAbs_EDGE, edges);
    return edges.Extent();
}
}

TEST_CASE("MergeFace feature merges selected faces and records one undo operation", "[MergeFacePlugin]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    UndoStack undo_stack(model_layer);
    model_layer.setUndoRecorder(&undo_stack);
    FeatureSystem feature_system(model_layer, bus, &undo_stack);

    const MergeFaceFixture fixture = addMergeableFaces(model_layer);
    undo_stack.clear();
    FeatureSystem::SystemHandlerPtr handler { new MergeFaceHandler };
    REQUIRE(feature_system.registerHandler(handlerMetaData(), std::move(handler)));

    const std::any hint = feature_system.invoke("MergeFace");
    REQUIRE(std::any_cast<std::string>(hint)
        == "请选择两个或多个需要合并的几何面。");
    REQUIRE(feature_system.setParameter("MergeFace", 0,
        core::ArgObject::create<ArgTypeEnum::Selector>(makeFaceSelection(fixture))));

    const std::string result = std::any_cast<std::string>(feature_system.invoke("MergeFace"));
    REQUIRE(result.find("成功") != std::string::npos);
    REQUIRE(countFaces(*model_layer.findComponent(fixture.component_id)) == 6);
    REQUIRE(countEdges(*model_layer.findComponent(fixture.component_id)) == 12);
    REQUIRE(countMergedBottomFaceEdges(*model_layer.findComponent(fixture.component_id)) == 4);
    REQUIRE(undo_stack.undoLabel() == "合并面");

    undo_stack.undo();
    REQUIRE(countFaces(*model_layer.findComponent(fixture.component_id)) == 7);
    REQUIRE(countEdges(*model_layer.findComponent(fixture.component_id)) == 13);
    undo_stack.redo();
    REQUIRE(countFaces(*model_layer.findComponent(fixture.component_id)) == 6);
    REQUIRE(countEdges(*model_layer.findComponent(fixture.component_id)) == 12);
    REQUIRE(countMergedBottomFaceEdges(*model_layer.findComponent(fixture.component_id)) == 4);
}
