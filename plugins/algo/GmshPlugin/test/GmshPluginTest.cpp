#include <catch2/catch_test_macros.hpp>

#include "GmshMeshHandler.h"
#include "IncrementalMeshTools.h"

#include "ArgObject.h"
#include "ComponentData.h"
#include "GeometryData.h"
#include "ModelIOSystemBase.h"
#include "ModelLayer.h"
#include "Selection.h"

#include <BRepPrimAPI_MakeBox.hxx>
#include <TopoDS_Shape.hxx>

#include <spdlog/spdlog.h>

namespace {

// 测试用 IO 系统，只记录插件读写请求，避免真实导入导出影响用例状态。
class MockIOSystem : public systems::io::ModelIOSystemBase {
public:
    void read(const std::filesystem::path& path, const std::string& file_type, const std::vector<std::any>& args) override
    {
        spdlog::info("[MockIO] read file: {}", path.string());
    }

    void write(Index model, const std::filesystem::path& path, const std::string& file_type, const std::vector<std::any>& args) override
    {
        spdlog::info("[MockIO] write model to: {}", path.string());
    }

    void writeComponents(const std::vector<Index>& component_ids,
        const std::filesystem::path& path,
        const std::string& file_type,
        const std::vector<std::any>& args) override
    {
        spdlog::info("[MockIO] write components to: {}", path.string());
    }
};

} // namespace

TEST_CASE("GmshMeshHandler rebuilds shared edge state across executions", "[GmshPlugin]")
{
    spdlog::set_level(spdlog::level::info);

    BRepPrimAPI_MakeBox boxMaker(10.0, 10.0, 10.0);
    boxMaker.Build();
    REQUIRE(boxMaker.IsDone() == true);

    auto geometryData = std::make_unique<GeometryData>();
    geometryData->rootShape = std::make_unique<TopoDS_Shape>(boxMaker.Shape());

    ComponentDatas components;
    auto component = std::make_unique<ComponentData>();
    component->name = "box";
    component->geometry = std::move(geometryData);
    components.push_back(std::move(component));

    ModelLayer modelLayer;
    Index modelId = modelLayer.addModel("box", std::move(components));
    auto componentIds = modelLayer.modelById(modelId)->componentIds();
    REQUIRE(componentIds.size() == 1);

    auto componentOp = modelLayer.getComponentOperator(componentIds[0]);
    REQUIRE(componentOp.has_value());

    MockIOSystem mockIo;
    systems::algo::HandlerContext context { mockIo, *componentOp };

    ComponentData* comp = modelLayer.findComponent(componentIds[0]);
    REQUIRE(comp != nullptr);
    comp->geometry->ensureIndexBuilt(modelLayer.geomRegistry());
    auto selection = std::make_shared<Selection>();
    selection->type = ElementEnum::GeometryFace;
    selection->ids = { comp->geometry->index.faceGlobalId(1) };

    std::vector<core::ArgObject> args;
    args.push_back(core::ArgObject::create<ArgTypeEnum::Selector>(selection));
    args.push_back(core::ArgObject::create<ArgTypeEnum::Text>("2.0"));

    systems::algo::GmshMeshHandler handler;
    REQUIRE(handler.resolveComponentId(modelLayer, -1, args) == componentIds[0]);
    handler.execute(context, args);

    comp = modelLayer.findComponent(componentIds[0]);
    REQUIRE(comp != nullptr);
    REQUIRE(comp->mesh != nullptr);
    REQUIRE(comp->mapping != nullptr);
    const std::size_t first_face_cell_count = comp->mesh->face_vertices_offset_.size() - 1;
    REQUIRE(first_face_cell_count > 0);
    REQUIRE(comp->mapping->geometry_face_to_mesh_topology.size() == 1);

    // 第二次独立执行划分相邻面，必须由 GeometryMeshMap 重建共享边状态。
    selection->ids = { comp->geometry->index.faceGlobalId(3) };
    handler.execute(context, args);

    comp = modelLayer.findComponent(componentIds[0]);
    REQUIRE(comp != nullptr);
    REQUIRE(comp->mesh != nullptr);
    REQUIRE(comp->geometry != nullptr);
    REQUIRE(comp->mapping != nullptr);
    REQUIRE_FALSE(comp->point_global_ids_.empty());
    REQUIRE(comp->point_global_ids_.size() == comp->mesh->vertex_positions_.size());
    REQUIRE_FALSE(comp->mesh->face_vertices_.empty());
    REQUIRE(comp->mesh->face_vertices_offset_.size() - 1 > first_face_cell_count);
    REQUIRE(comp->mapping->geometry_face_to_mesh_topology.size() == 2);

    // 删除第二个面时直接使用通用面映射，并只保留第一个面仍在使用的边缓存。
    const std::size_t two_face_edge_count =
        comp->mapping->geometry_edge_to_mesh_point_ids.size();
    args.push_back(core::ArgObject::create<ArgTypeEnum::Text>("2"));
    handler.execute(context, args);

    comp = modelLayer.findComponent(componentIds[0]);
    REQUIRE(comp->mesh->face_vertices_offset_.size() - 1 == first_face_cell_count);
    REQUIRE(comp->mapping->geometry_face_to_mesh_topology.size() == 1);
    REQUIRE(comp->mapping->geometry_edge_to_mesh_point_ids.size() < two_face_edge_count);
}

TEST_CASE("GmshMeshHandler rejects invalid current parameters", "[GmshPlugin]")
{
    BRepPrimAPI_MakeBox boxMaker(10.0, 10.0, 10.0);
    boxMaker.Build();
    REQUIRE(boxMaker.IsDone());

    auto geometryData = std::make_unique<GeometryData>();
    geometryData->rootShape = std::make_unique<TopoDS_Shape>(boxMaker.Shape());

    ComponentDatas components;
    auto component = std::make_unique<ComponentData>();
    component->geometry = std::move(geometryData);
    components.push_back(std::move(component));

    ModelLayer modelLayer;
    Index modelId = modelLayer.addModel("box", std::move(components));
    const Index componentId = modelLayer.modelById(modelId)->componentIds().front();
    auto componentOp = modelLayer.getComponentOperator(componentId);
    REQUIRE(componentOp.has_value());

    MockIOSystem mockIo;
    systems::algo::HandlerContext context { mockIo, *componentOp };
    ComponentData* comp = modelLayer.findComponent(componentId);
    REQUIRE(comp != nullptr);
    comp->geometry->ensureIndexBuilt(modelLayer.geomRegistry());
    auto selection = std::make_shared<Selection>();
    selection->type = ElementEnum::GeometryFace;
    selection->ids = { comp->geometry->index.faceGlobalId(1) };

    std::vector<core::ArgObject> args {
        core::ArgObject::create<ArgTypeEnum::Selector>(selection),
        core::ArgObject::create<ArgTypeEnum::Combo>(0),
        core::ArgObject::create<ArgTypeEnum::Text>("1.0"),
        core::ArgObject::create<ArgTypeEnum::Text>("2.0"),
        core::ArgObject::create<ArgTypeEnum::Text>("1.0"),
        core::ArgObject::create<ArgTypeEnum::Combo>(0),
        core::ArgObject::create<ArgTypeEnum::Combo>(0),
        core::ArgObject::create<ArgTypeEnum::Text>(""),
        core::ArgObject::create<ArgTypeEnum::Combo>(0),
        core::ArgObject::create<ArgTypeEnum::Text>(""),
        core::ArgObject::create<ArgTypeEnum::Text>(""),
        core::ArgObject::create<ArgTypeEnum::Bool>(false)
    };

    systems::algo::GmshMeshHandler handler;
    handler.execute(context, args);

    comp = modelLayer.findComponent(componentId);
    REQUIRE(comp != nullptr);
    REQUIRE(comp->mesh == nullptr);
}
