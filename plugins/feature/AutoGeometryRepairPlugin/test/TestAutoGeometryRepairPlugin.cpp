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
 * @brief 自动间隙修复测试数据，保存选择器直接使用的组件 ID。
 */
struct RepairFixture {
    Index component_id { -1 };
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
    return { component_id };
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
    const RepairFixture other = addGappedFaces(model_layer);

    FeatureSystem::SystemHandlerPtr handler { new AutoGeometryRepairHandler };
    REQUIRE(feature_system.registerHandler(handlerMetaData(), std::move(handler)));
    const auto navigation_entries = feature_system.getNavigationEntries();
    REQUIRE(navigation_entries.size() == 1);
    CHECK(navigation_entries.front().id == "AutoGeometryRepair");
    CHECK(navigation_entries.front().title == "自动修复间隙");
    CHECK(navigation_entries.front().menu_path == "几何/修复");
    auto selection = std::make_shared<Selection>();
    selection->type = ElementEnum::Component;
    // component_id 是其他选择类型的归属提示；组件选择必须使用 ids 中的明确目标。
    selection->component_id = other.component_id;
    selection->ids = { fixture.component_id };
    REQUIRE(feature_system.setParameter("AutoGeometryRepair", 0,
        core::ArgObject::create<ArgTypeEnum::Selector>(selection)));
    REQUIRE(feature_system.setParameter("AutoGeometryRepair", 1,
        core::ArgObject::create<ArgTypeEnum::Float>(0.01)));

    const auto infos = feature_system.getFeatureInfos();
    REQUIRE(infos.size() == 1);
    REQUIRE(infos.front()->arg_types.front().type == ArgTypeEnum::Selector);
    REQUIRE(infos.front()->arg_types.front().content == "Component");
    const int untouched = countBoundaryEdges(*model_layer.findComponent(other.component_id));
    const int original = countBoundaryEdges(*model_layer.findComponent(fixture.component_id));
    const std::string detection = std::any_cast<std::string>(
        feature_system.invoke("AutoGeometryRepair"));
    REQUIRE(detection.find("检测到 1 对") != std::string::npos);
    REQUIRE(countBoundaryEdges(*model_layer.findComponent(fixture.component_id)) == original);

    // 修改本插件参数后检测立即使用新容差，不依赖模型层的会话状态。
    REQUIRE(feature_system.setParameter("AutoGeometryRepair", 1,
        core::ArgObject::create<ArgTypeEnum::Float>(0.001)));
    const auto narrow_detection = std::any_cast<std::string>(feature_system.invoke("AutoGeometryRepair"));
    REQUIRE(narrow_detection.find("检测到 0 对") != std::string::npos);
    REQUIRE(countBoundaryEdges(*model_layer.findComponent(fixture.component_id)) == original);
    REQUIRE(feature_system.setParameter("AutoGeometryRepair", 1,
        core::ArgObject::create<ArgTypeEnum::Float>(0.01)));

    REQUIRE(feature_system.setParameter("AutoGeometryRepair", 2,
        core::ArgObject::create<ArgTypeEnum::Combo>(1)));
    const int before = countBoundaryEdges(*model_layer.findComponent(fixture.component_id));
    const std::string result = std::any_cast<std::string>(feature_system.invoke("AutoGeometryRepair"));
    REQUIRE(result.find("成功") != std::string::npos);
    REQUIRE(countBoundaryEdges(*model_layer.findComponent(fixture.component_id)) < before);
    REQUIRE(countBoundaryEdges(*model_layer.findComponent(other.component_id)) == untouched);
}

//! @brief 拒绝旧的面选择、多组件、空选择及失效组件，避免不明确地扩大操作范围。
TEST_CASE("AutoGeometryRepair requires one explicit valid component", "[AutoGeometryRepairPlugin]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem feature_system(model_layer, bus);
    const auto fixture = addGappedFaces(model_layer);
    const auto other = addGappedFaces(model_layer);
    FeatureSystem::SystemHandlerPtr handler { new AutoGeometryRepairHandler };
    REQUIRE(feature_system.registerHandler(handlerMetaData(), std::move(handler)));
    REQUIRE(feature_system.setParameter("AutoGeometryRepair", 2,
        core::ArgObject::create<ArgTypeEnum::Combo>(1)));
    for (int invalid_case = 0; invalid_case < 4; ++invalid_case) {
        CAPTURE(invalid_case);
        auto selection = std::make_shared<Selection>();
        selection->type = invalid_case == 0 ? ElementEnum::GeometryFace : ElementEnum::Component;
        selection->ids = { fixture.component_id };
        if (invalid_case == 1)
            selection->ids.clear();
        if (invalid_case == 2)
            selection->ids.push_back(other.component_id);
        if (invalid_case == 3)
            selection->ids = { -1 };
        REQUIRE(feature_system.setParameter("AutoGeometryRepair", 0,
            core::ArgObject::create<ArgTypeEnum::Selector>(selection)));
        const auto message = std::any_cast<std::string>(feature_system.invoke("AutoGeometryRepair"));
        REQUIRE(message == (invalid_case == 3 ? "所选组件已失效。" : "请选择一个几何组件。"));
        REQUIRE(countBoundaryEdges(*model_layer.findComponent(fixture.component_id)) == 8);
        REQUIRE(countBoundaryEdges(*model_layer.findComponent(other.component_id)) == 8);
    }
}
