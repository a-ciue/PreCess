#include <catch2/catch_test_macros.hpp>

#include "GmshMeshHandler.h"

#include "ArgObject.h"
#include "ComponentData.h"
#include "GeometryData.h"
#include "ModelData.h"
#include "ModelIOSystemBase.h"
#include "ModelLayer.h"

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

TEST_CASE("GmshMeshHandler Execution Test", "[GmshPlugin]")
{
    spdlog::set_level(spdlog::level::info);

    BRepPrimAPI_MakeBox boxMaker(10.0, 10.0, 10.0);
    boxMaker.Build();
    REQUIRE(boxMaker.IsDone() == true);

    auto geometryData = std::make_unique<GeometryData>();
    geometryData->rootShape = std::make_unique<TopoDS_Shape>(boxMaker.Shape());

    ModelLayer modelLayer;
    Index modelId = modelLayer.addModel(std::make_unique<ModelData>(std::move(geometryData)));
    auto componentIds = modelLayer.getComponentIds(modelId);
    REQUIRE(componentIds.size() == 1);

    auto componentOp = modelLayer.getComponentOperator(componentIds[0]);
    REQUIRE(componentOp.has_value());

    MockIOSystem mockIo;
    systems::algo::HandlerContext context { mockIo, *componentOp };

    std::vector<core::ArgObject> args;
    args.push_back(core::ArgObject::create<ArgTypeEnum::Text>("0"));
    args.push_back(core::ArgObject::create<ArgTypeEnum::Text>("2.0"));

    systems::algo::GmshMeshHandler handler;
    handler.execute(context, args);

    ComponentData* comp = modelLayer.findComponent(componentIds[0]);
    REQUIRE(comp != nullptr);
    REQUIRE(comp->mesh != nullptr);
    REQUIRE(comp->geometry != nullptr);
    REQUIRE_FALSE(comp->mesh->vertex_positions_.empty());
    REQUIRE_FALSE(comp->mesh->face_vertices_.empty());
    REQUIRE(comp->mesh->face_vertices_offset_.size() > 1);
}
