#include <catch2/catch_test_macros.hpp>

#include "GmshMeshHandler.h"
#include "IncrementalMeshContext.h"
#include "IncrementalMeshTools.h"

#include "ArgObject.h"
#include "ModelData.h"
#include "ModelIOSystemBase.h"
#include "ModelOperator.h"
#include "SplineData.h"

#include <BRepPrimAPI_MakeBox.hxx>
#include <TopoDS_Shape.hxx>

#include <spdlog/spdlog.h>

namespace {

// 模拟的 IO 系统，避免在测试中涉及真实的界面交互或不可控流
class MockIOSystem : public systems::io::ModelIOSystemBase {
public:
    void read(const std::filesystem::path& path, const std::string& file_type, const std::vector<std::any>& args) override
    {
        spdlog::info("[MockIO] read file: {}", path.string());
    }
    void write(Index model, const std::filesystem::path& path, const std::string& file_type, const std::vector<std::any>& args) override
    {
        spdlog::info("[MockIO] write to: {}", path.string());
    }
};

} // namespace

TEST_CASE("GmshMeshHandler Execution Test", "[GmshPlugin]")
{
    spdlog::set_level(spdlog::level::info);

    // 1. 使用 OCC 构造一个 10x10x10 的立方体形状
    BRepPrimAPI_MakeBox boxMaker(10.0, 10.0, 10.0);
    boxMaker.Build();
    REQUIRE(boxMaker.IsDone() == true);
    TopoDS_Shape cubeShape = boxMaker.Shape();

    // 2. 初始化 SplineData 及持有它的 ModelData
    auto splineData = std::make_unique<SplineData>();
    splineData->rootShape = std::make_unique<TopoDS_Shape>(cubeShape);

    ModelData modelData(std::move(splineData));
    ModelOperator op(1, modelData);

    // 3. 构建模拟的 IO 系统和上下文环境
    MockIOSystem mockIo;
    systems::algo::HandlerContext context { mockIo, op };

    // 4. 准备入参：
    // Arg 0: 面索引 "0"
    // Arg 1: 网格尺寸 "2.0"
    std::vector<core::ArgObject> args;
    args.push_back(core::ArgObject::create<ArgTypeEnum::Text>("0"));
    args.push_back(core::ArgObject::create<ArgTypeEnum::Text>("2.0"));

    // 5. 调用执行插件方法
    systems::algo::GmshMeshHandler handler;
    std::any result = handler.execute(context, args);

    // 6. 后置断言：验证执行后的内部状态变化
    SplineData* sp = op.data().asSplineData();
    REQUIRE(sp != nullptr);
    REQUIRE(sp->meshContext != nullptr);

    // 立方体应当被提取到了 6 个面
    REQUIRE(sp->meshContext->faceCount() == 6);

    // 由于面 0 已经成功完成了网格划分，其关联的 4 条自由边界应该被缓存起来了
    REQUIRE(sp->meshedEdgeRefCounts.size() == 4);
    REQUIRE(sp->meshedEdgesCache.size() == 4);
}