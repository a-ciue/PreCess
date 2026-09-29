#include "AutoGeometryRepairHandler.h"
#include "ComponentData.h"
#include "EventBus.h"
#include "FeatureSystem.h"
#include "GeometryBuilder.h"
#include "GeometryData.h"
#include "ModelLayer.h"
#include "ModelOperator.h"
#include "Selection.h"

#include <BRep_Builder.hxx>
#include <NCollection_IndexedDataMap.hxx>
#include <NCollection_List.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Face.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <catch2/catch_test_macros.hpp>

#include <any>
#include <memory>
#include <string>
#include <utility>

using namespace systems;
using namespace systems::feature;

namespace {
/**
 * @brief 自动间隙修复测试数据，保存组件和用于定位组件的面 ID。
 */
struct RepairFixture {
    Index component_id { -1 };
    GeomFaceId face_id { kInvalidGeomFaceId };
};

HandlerMetaData handlerMetaData()
{
    HandlerMetaData meta_data;
    meta_data.name = "AutoGeometryRepair";
    meta_data.display_name = "自动修复间隙";
    return meta_data;
}

RepairFixture addGappedFaces(ModelLayer& model_layer)
{
    const TopoDS_Face first = TopoDS::Face(
        GeometryBuilder::makeRectangleFace(
            0.0, 0.0, 0.0, 10.0, 5.0, CoordinatePlane::XY));
    const TopoDS_Face second = TopoDS::Face(
        GeometryBuilder::makeRectangleFace(
            10.005, 0.0, 0.0, 10.0, 5.0, CoordinatePlane::XY));
    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    builder.Add(compound, first);
    builder.Add(compound, second);

    auto geometry = std::make_unique<GeometryData>();
    geometry->setRootShape(compound);
    auto component = std::make_unique<ComponentData>();
    component->geometry = std::move(geometry);
    const Index model_id = model_layer.addModel("repair_gap_test", {});
    auto model_operator = model_layer.getModelOperator(model_id);
    REQUIRE(model_operator.has_value());
    const Index component_id = model_operator->addGeometryComponent(std::move(component));

    ComponentData* stored = model_layer.findComponent(component_id);
    REQUIRE(stored != nullptr);
    stored->geometry->ensureIndexBuilt(model_layer.geomRegistry());
    const auto& index = stored->geometry->index;
    const int face_local = index.type_maps[index.typeIndex(TopAbs_FACE)].FindIndex(first);
    REQUIRE(face_local > 0);
    return { component_id, index.faceGlobalId(face_local) };
}

int countBoundaryEdges(const ComponentData& component)
{
    NCollection_IndexedDataMap<TopoDS_Shape,
        NCollection_List<TopoDS_Shape>, TopTools_ShapeMapHasher>
        edge_faces;
    TopExp::MapShapesAndUniqueAncestors(
        *component.geometry->rootShape, TopAbs_EDGE, TopAbs_FACE, edge_faces);
    int count = 0;
    for (int index = 1; index <= edge_faces.Extent(); ++index) {
        if (edge_faces.FindFromIndex(index).Extent() == 1)
            ++count;
    }
    return count;
}
}

TEST_CASE("AutoGeometryRepair feature detects then repairs free edge gaps", "[AutoGeometryRepairPlugin]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem feature_system(model_layer, bus);
    const RepairFixture fixture = addGappedFaces(model_layer);

    FeatureSystem::SystemHandlerPtr handler { new AutoGeometryRepairHandler };
    REQUIRE(feature_system.registerHandler(handlerMetaData(), std::move(handler)));
    auto selection = std::make_shared<Selection>();
    selection->type = ElementEnum::GeometryFace;
    selection->component_id = fixture.component_id;
    selection->ids = { fixture.face_id };
    REQUIRE(feature_system.setParameter("AutoGeometryRepair", 0,
        core::ArgObject::create<ArgTypeEnum::Selector>(selection)));
    REQUIRE(feature_system.setParameter("AutoGeometryRepair", 1,
        core::ArgObject::create<ArgTypeEnum::Float>(0.01)));

    const std::string detection = std::any_cast<std::string>(
        feature_system.invoke("AutoGeometryRepair"));
    REQUIRE(detection.find("检测到 1 对") != std::string::npos);
    REQUIRE(model_layer.geometryCleanupTolerance() == 0.01);

    REQUIRE(feature_system.setParameter("AutoGeometryRepair", 2,
        core::ArgObject::create<ArgTypeEnum::Combo>(1)));
    const int before = countBoundaryEdges(*model_layer.findComponent(fixture.component_id));
    const Index result = std::any_cast<Index>(feature_system.invoke("AutoGeometryRepair"));
    REQUIRE(result == fixture.component_id);
    REQUIRE(countBoundaryEdges(*model_layer.findComponent(fixture.component_id)) < before);
}
