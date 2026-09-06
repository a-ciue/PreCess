/**
 * @file TestMeshBooleanPlugin.cpp
 * @brief 网格布尔 Feature 插件单元测试
 *
 * FeatureSystem 驱动模式：通过两个 Selector 参数显式传入对象 A / B，验证
 *   1) 参数声明（两个 Selector + 运算 Combo）
 *   2) 未选对象 / 选择相同对象的兜底提示
 *   3) 并集 / 交集 / 差集(A−B) 在规则重叠盒体上的几何正确性（结果均为独立新模型）
 *   4) 表面不相交时的退化场景：分离 → 合并壳体 / 空；包含 → 结果即被包含方
 *      （有结果则新建模型，结果为空则不创建）
 *   5) 非闭合网格被预检拒绝（不生成新模型）
 *
 * 测试网格为封闭三角盒体（8 顶点 / 12 三角面），通过平移生成 A、B 两个操作对象。
 */

#include "ArgObject.h"
#include "ArgType.h"
#include "ComponentData.h"
#include "Core.h"
#include "EventBus.h"
#include "FeatureInfo.h"
#include "FeatureSystem.h"
#include "MeshBooleanHandler.h"
#include "MeshData.h"
#include "ModelData.h"
#include "ModelLayer.h"
#include "Selection.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <any>
#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

using namespace systems;
using namespace systems::feature;

namespace {

//! @brief 网格包围盒
struct BBox {
    std::array<double, 3> mn;
    std::array<double, 3> mx;
};

//! @brief 计算 MeshData 顶点包围盒
BBox meshBBox(const MeshData& mesh)
{
    REQUIRE(!mesh.vertex_positions_.empty());
    BBox box { mesh.vertex_positions_.front(), mesh.vertex_positions_.front() };
    for (const auto& p : mesh.vertex_positions_) {
        for (int d = 0; d < 3; ++d) {
            box.mn[d] = std::min(box.mn[d], p[d]);
            box.mx[d] = std::max(box.mx[d], p[d]);
        }
    }
    return box;
}

//! @brief 是否存在距目标点 < eps 的顶点
bool hasVertexNear(const MeshData& mesh, const std::array<double, 3>& target, double eps = 1e-6)
{
    for (const auto& p : mesh.vertex_positions_) {
        const auto dx = p[0] - target[0];
        const auto dy = p[1] - target[1];
        const auto dz = p[2] - target[2];
        if (dx * dx + dy * dy + dz * dz < eps * eps)
            return true;
    }
    return false;
}

//! @brief 面总数
std::size_t faceCount(const MeshData& mesh)
{
    return mesh.face_vertices_offset_.empty() ? 0 : mesh.face_vertices_offset_.size() - 1;
}

/**
 * @brief 构造一个封闭（水密）三角盒体 Component，盒体范围 [origin, origin+s]^3
 *
 * 顶点与面的方向沿用单位立方体的外部朝外布局，仅坐标平移。
 */
Index addClosedBoxComponent(ModelLayer& model_layer, const std::string& name,
    double ox, double oy, double oz, double s)
{
    auto mesh = std::make_unique<MeshData>();
    mesh->init();
    mesh->vertex_positions_ = {
        { ox, oy, oz },                         // 0
        { ox + s, oy, oz },                     // 1
        { ox + s, oy + s, oz },                 // 2
        { ox, oy + s, oz },                     // 3
        { ox, oy, oz + s },                     // 4
        { ox + s, oy, oz + s },                 // 5
        { ox + s, oy + s, oz + s },             // 6
        { ox, oy + s, oz + s },                 // 7
    };
    mesh->face_vertices_ = {
        // 底 z=0
        0, 2, 1,  0, 3, 2,
        // 顶 z=s
        4, 5, 6,  4, 6, 7,
        // 前 y=0
        0, 1, 5,  0, 5, 4,
        // 后 y=s
        3, 7, 6,  3, 6, 2,
        // 左 x=0
        0, 4, 7,  0, 7, 3,
        // 右 x=s
        1, 2, 6,  1, 6, 5,
    };
    mesh->face_vertices_offset_ = { 0, 3, 6, 9, 12, 15, 18, 21, 24, 27, 30, 33, 36 };

    auto component = std::make_unique<ComponentData>();
    component->name = name;
    component->mesh = std::move(mesh);
    ComponentDatas components;
    components.push_back(std::move(component));

    const Index model_id = model_layer.addModel(name + "_Model", std::move(components));
    return model_layer.modelById(model_id)->componentIds().front();
}

/**
 * @brief 构造一个非闭合盒体 Component（去掉顶面 2 个三角面）→ 触发"不是闭合网格"预检
 */
Index addOpenBoxComponent(ModelLayer& model_layer, const std::string& name)
{
    auto mesh = std::make_unique<MeshData>();
    mesh->init();
    mesh->vertex_positions_ = {
        { 0.0, 0.0, 0.0 }, // 0
        { 1.0, 0.0, 0.0 }, // 1
        { 1.0, 1.0, 0.0 }, // 2
        { 0.0, 1.0, 0.0 }, // 3
        { 0.0, 0.0, 1.0 }, // 4
        { 1.0, 0.0, 1.0 }, // 5
        { 1.0, 1.0, 1.0 }, // 6
        { 0.0, 1.0, 1.0 }, // 7
    };
    // 缺少顶面 (4,5,6) / (4,6,7) → 边界环 z=1，非闭合
    mesh->face_vertices_ = {
        0, 2, 1,  0, 3, 2,
        0, 1, 5,  0, 5, 4,
        3, 7, 6,  3, 6, 2,
        0, 4, 7,  0, 7, 3,
        1, 2, 6,  1, 6, 5,
    };
    mesh->face_vertices_offset_ = { 0, 3, 6, 9, 12, 15, 18, 21, 24, 27, 30 };

    auto component = std::make_unique<ComponentData>();
    component->name = name;
    component->mesh = std::move(mesh);
    ComponentDatas components;
    components.push_back(std::move(component));

    const Index model_id = model_layer.addModel(name + "_Model", std::move(components));
    return model_layer.modelById(model_id)->componentIds().front();
}

//! @brief 构造测试用的 MeshBoolean HandlerMetaData
HandlerMetaData booleanMetaData()
{
    HandlerMetaData metadata;
    metadata.name = "MeshBoolean";
    metadata.display_name = "网格布尔";
    return metadata;
}

//! @brief 创建指向目标 Component 的 Selector
std::shared_ptr<Selection> makeComponentSelection(Index component_id)
{
    auto selection = std::make_shared<Selection>();
    selection->type = ElementEnum::Component;
    selection->ids = { component_id };
    return selection;
}

//! @brief 注册插件并装配 A/B/运算 三个参数（返回可连续 setup 的引用便于链式断言）
struct Fixture {
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem feature_system { model_layer, bus };

    Fixture()
    {
        FeatureSystem::SystemHandlerPtr handler { new MeshBooleanHandler };
        REQUIRE(feature_system.registerHandler(booleanMetaData(), std::move(handler)));
    }

    void selectA(Index id)
    {
        REQUIRE(feature_system.setParameter(
            "MeshBoolean", 0,
            core::ArgObject::create<ArgTypeEnum::Selector>(makeComponentSelection(id))));
    }

    void selectB(Index id)
    {
        REQUIRE(feature_system.setParameter(
            "MeshBoolean", 1,
            core::ArgObject::create<ArgTypeEnum::Selector>(makeComponentSelection(id))));
    }

    void setOp(int op)
    {
        REQUIRE(feature_system.setParameter(
            "MeshBoolean", 2,
            core::ArgObject::create<ArgTypeEnum::Combo>(op)));
    }

    std::string invokeText()
    {
        const std::any result = feature_system.invoke("MeshBoolean");
        REQUIRE(result.type() == typeid(std::string));
        return std::any_cast<const std::string&>(result);
    }
};

//! @brief 按模型名查找模型（模型 id 由模型层分配，测试以名字定位布尔结果）
ModelData* findModelByName(ModelLayer& model_layer, const std::string& name)
{
    for (Index i = 0; i < 64; ++i) {
        ModelData* model = model_layer.modelById(i);
        if (model && model->model_name_ == name)
            return model;
    }
    return nullptr;
}

//! @brief 取结果模型中唯一组件的网格；模型不存在或组件数不为 1 时返回 nullptr
const MeshData* resultMeshOf(ModelLayer& model_layer, const std::string& name)
{
    const ModelData* model = findModelByName(model_layer, name);
    if (!model || model->componentIds().size() != 1)
        return nullptr;
    const ComponentData* component = model_layer.findComponent(model->componentIds().front());
    return component ? component->mesh.get() : nullptr;
}

} // namespace

TEST_CASE("MeshBoolean exposes two Selectors and operation Combo parameters", "[MeshBooleanPlugin]")
{
    Fixture fx;

    const auto infos = fx.feature_system.getFeatureInfos();
    REQUIRE(infos.size() == 1);
    REQUIRE(infos.front()->arg_types.size() == 3);

    // 参数 0：对象 A Selector（Component）
    REQUIRE(infos.front()->arg_types[0].type == ArgTypeEnum::Selector);
    REQUIRE(infos.front()->arg_types[0].content == "Component");
    // 参数 1：对象 B Selector（Component）
    REQUIRE(infos.front()->arg_types[1].type == ArgTypeEnum::Selector);
    REQUIRE(infos.front()->arg_types[1].content == "Component");
    // 参数 2：Combo 列出四种运算
    REQUIRE(infos.front()->arg_types[2].type == ArgTypeEnum::Combo);
    REQUIRE(infos.front()->arg_types[2].content.find("并集") != std::string::npos);
    REQUIRE(infos.front()->arg_types[2].content.find("交集") != std::string::npos);
    REQUIRE(infos.front()->arg_types[2].content.find("差集(A−B)") != std::string::npos);
    REQUIRE(infos.front()->arg_types[2].content.find("差集(B−A)") != std::string::npos);
}

TEST_CASE("MeshBoolean returns guidance when operands are missing or identical", "[MeshBooleanPlugin]")
{
    Fixture fx;
    const Index a = addClosedBoxComponent(fx.model_layer, "A", 0, 0, 0, 1.0);

    // 未设置任何对象 → 提示先选 A
    REQUIRE(fx.invokeText().find("请选择对象 A") != std::string::npos);

    fx.selectA(a);
    // 只选了 A，未选 B → 提示选 B
    REQUIRE(fx.invokeText().find("请选择对象 B") != std::string::npos);

    fx.selectB(a);
    // A 与 B 相同 → 拒绝
    REQUIRE(fx.invokeText().find("请选择两个不同的 Component") != std::string::npos);
}

TEST_CASE("MeshBoolean union of overlapping boxes generates a separate result model", "[MeshBooleanPlugin]")
{
    Fixture fx;
    // A = [0,1]^3, B = [0.5,1.5]^3（三维均重叠 0.5）
    const Index a = addClosedBoxComponent(fx.model_layer, "A", 0, 0, 0, 1.0);
    const Index b = addClosedBoxComponent(fx.model_layer, "B", 0.5, 0.5, 0.5, 1.0);

    fx.selectA(a);
    fx.selectB(b);
    fx.setOp(0); // 并集
    const std::string text = fx.invokeText();

    REQUIRE(text.find("并集完成") != std::string::npos);
    REQUIRE(text.find("已生成新模型") != std::string::npos);

    // 结果落在独立的新模型中：包围盒 = [0, 1.5]^3
    const MeshData* result = resultMeshOf(fx.model_layer, "A_并集_B");
    REQUIRE(result != nullptr);
    const BBox box = meshBBox(*result);
    for (int d = 0; d < 3; ++d) {
        REQUIRE(box.mn[d] == Catch::Approx(0.0).margin(1e-6));
        REQUIRE(box.mx[d] == Catch::Approx(1.5).margin(1e-6));
    }
    // 顶点数应多于任一输入盒体（并集体包含 0.5 交线角点）
    REQUIRE(result->vertex_count_ > 8);
    // 两个操作数均保持原样
    REQUIRE(fx.model_layer.findComponent(a)->mesh->vertex_count_ == 8);
    REQUIRE(fx.model_layer.findComponent(b)->mesh->vertex_count_ == 8);
}

TEST_CASE("MeshBoolean intersection of overlapping boxes", "[MeshBooleanPlugin]")
{
    Fixture fx;
    const Index a = addClosedBoxComponent(fx.model_layer, "A", 0, 0, 0, 1.0);
    const Index b = addClosedBoxComponent(fx.model_layer, "B", 0.5, 0.5, 0.5, 1.0);

    fx.selectA(a);
    fx.selectB(b);
    fx.setOp(1); // 交集
    const std::string text = fx.invokeText();

    REQUIRE(text.find("交集完成") != std::string::npos);
    REQUIRE(text.find("已生成新模型") != std::string::npos);

    // 交集 = [0.5, 1]^3：所有顶点都应落在此包围盒内，两个对角点应存在
    const MeshData* result = resultMeshOf(fx.model_layer, "A_交集_B");
    REQUIRE(result != nullptr);
    const BBox box = meshBBox(*result);
    for (int d = 0; d < 3; ++d) {
        REQUIRE(box.mn[d] >= 0.5 - 1e-6);
        REQUIRE(box.mx[d] <= 1.0 + 1e-6);
    }
    REQUIRE(hasVertexNear(*result, { 0.5, 0.5, 0.5 }));
    REQUIRE(hasVertexNear(*result, { 1.0, 1.0, 1.0 }));
    // 两个操作数均保持原样
    REQUIRE(fx.model_layer.findComponent(a)->mesh->vertex_count_ == 8);
    REQUIRE(fx.model_layer.findComponent(b)->mesh->vertex_count_ == 8);
}

TEST_CASE("MeshBoolean difference (A minus B) removes the overlapped corner", "[MeshBooleanPlugin]")
{
    Fixture fx;
    const Index a = addClosedBoxComponent(fx.model_layer, "A", 0, 0, 0, 1.0);
    const Index b = addClosedBoxComponent(fx.model_layer, "B", 0.5, 0.5, 0.5, 1.0);

    fx.selectA(a);
    fx.selectB(b);
    fx.setOp(2); // 差集(A−B)
    const std::string text = fx.invokeText();

    REQUIRE(text.find("差集(A−B)完成") != std::string::npos);
    REQUIRE(text.find("已生成新模型") != std::string::npos);

    const MeshData* result = resultMeshOf(fx.model_layer, "A_差集A-B_B");
    REQUIRE(result != nullptr);
    // 顶点都在 A 的包围盒内
    const BBox box = meshBBox(*result);
    for (int d = 0; d < 3; ++d) {
        REQUIRE(box.mn[d] >= -1e-6);
        REQUIRE(box.mx[d] <= 1.0 + 1e-6);
    }
    // (0,0,0) 角保留；(1,1,1) 角位于 B 内被挖除
    REQUIRE(hasVertexNear(*result, { 0.0, 0.0, 0.0 }));
    REQUIRE_FALSE(hasVertexNear(*result, { 1.0, 1.0, 1.0 }));
    // 结果不再是原始 8 顶点盒体
    REQUIRE(result->vertex_count_ > 8);
    // 两个操作数均保持原样
    REQUIRE(fx.model_layer.findComponent(a)->mesh->vertex_count_ == 8);
    REQUIRE(fx.model_layer.findComponent(b)->mesh->vertex_count_ == 8);
}

TEST_CASE("MeshBoolean union of separated boxes keeps two shells", "[MeshBooleanPlugin]")
{
    Fixture fx;
    const Index a = addClosedBoxComponent(fx.model_layer, "A", 0, 0, 0, 1.0);
    const Index b = addClosedBoxComponent(fx.model_layer, "B", 3, 3, 3, 1.0);

    fx.selectA(a);
    fx.selectB(b);
    fx.setOp(0); // 并集
    const std::string text = fx.invokeText();

    REQUIRE(text.find("并集完成") != std::string::npos);
    REQUIRE(text.find("两个独立壳体") != std::string::npos);

    // 两壳并入结果模型：面数 12+12=24，包围盒跨越两盒
    const MeshData* result = resultMeshOf(fx.model_layer, "A_并集_B");
    REQUIRE(result != nullptr);
    REQUIRE(faceCount(*result) == 24);
    const BBox box = meshBBox(*result);
    for (int d = 0; d < 3; ++d) {
        REQUIRE(box.mn[d] == Catch::Approx(0.0).margin(1e-6));
        REQUIRE(box.mx[d] == Catch::Approx(4.0).margin(1e-6));
    }
    // 两个操作数均保持原样
    REQUIRE(fx.model_layer.findComponent(a)->mesh->vertex_count_ == 8);
    REQUIRE(fx.model_layer.findComponent(b)->mesh->vertex_count_ == 8);
}

TEST_CASE("MeshBoolean intersection of separated boxes reports empty without modification", "[MeshBooleanPlugin]")
{
    Fixture fx;
    const Index a = addClosedBoxComponent(fx.model_layer, "A", 0, 0, 0, 1.0);
    const Index b = addClosedBoxComponent(fx.model_layer, "B", 3, 3, 3, 1.0);

    fx.selectA(a);
    fx.selectB(b);
    fx.setOp(1); // 交集
    const std::string text = fx.invokeText();

    REQUIRE(text.find("交集为空") != std::string::npos);

    // 结果为空 → 不生成新模型
    REQUIRE(resultMeshOf(fx.model_layer, "A_交集_B") == nullptr);
    // 两个操作数均保持原样
    REQUIRE(fx.model_layer.findComponent(a)->mesh->vertex_count_ == 8);
    REQUIRE(fx.model_layer.findComponent(b)->mesh->vertex_count_ == 8);
}

TEST_CASE("MeshBoolean intersection where B is fully inside A equals B", "[MeshBooleanPlugin]")
{
    Fixture fx;
    // A = [0,3]^3 大盒，B = [1,2]^3 小盒（完全位于 A 内，表面不相交）
    const Index a = addClosedBoxComponent(fx.model_layer, "A", 0, 0, 0, 3.0);
    const Index b = addClosedBoxComponent(fx.model_layer, "B", 1, 1, 1, 1.0);

    fx.selectA(a);
    fx.selectB(b);
    fx.setOp(1); // 交集
    const std::string text = fx.invokeText();

    REQUIRE(text.find("结果即对象 B") != std::string::npos);

    // 交集 = B，作为新模型生成
    const MeshData* result = resultMeshOf(fx.model_layer, "A_交集_B");
    REQUIRE(result != nullptr);
    const BBox box = meshBBox(*result);
    for (int d = 0; d < 3; ++d) {
        REQUIRE(box.mn[d] == Catch::Approx(1.0).margin(1e-6));
        REQUIRE(box.mx[d] == Catch::Approx(2.0).margin(1e-6));
    }
    // 两个操作数均保持原样
    REQUIRE(fx.model_layer.findComponent(a)->mesh->vertex_count_ == 8);
    REQUIRE(fx.model_layer.findComponent(b)->mesh->vertex_count_ == 8);
}

TEST_CASE("MeshBoolean union where B is fully inside A keeps A unchanged", "[MeshBooleanPlugin]")
{
    Fixture fx;
    const Index a = addClosedBoxComponent(fx.model_layer, "A", 0, 0, 0, 3.0);
    const Index b = addClosedBoxComponent(fx.model_layer, "B", 1, 1, 1, 1.0);

    fx.selectA(a);
    fx.selectB(b);
    fx.setOp(0); // 并集
    const std::string text = fx.invokeText();

    REQUIRE(text.find("结果即对象 A") != std::string::npos);

    // 并集 = A，作为新模型生成（内容为大盒 A）
    const MeshData* result = resultMeshOf(fx.model_layer, "A_并集_B");
    REQUIRE(result != nullptr);
    const BBox box = meshBBox(*result);
    for (int d = 0; d < 3; ++d) {
        REQUIRE(box.mn[d] == Catch::Approx(0.0).margin(1e-6));
        REQUIRE(box.mx[d] == Catch::Approx(3.0).margin(1e-6));
    }
    // 两个操作数均保持原样
    REQUIRE(fx.model_layer.findComponent(a)->mesh->vertex_count_ == 8);
    REQUIRE(fx.model_layer.findComponent(b)->mesh->vertex_count_ == 8);
}

TEST_CASE("MeshBoolean rejects non-closed meshes before CGAL computation", "[MeshBooleanPlugin]")
{
    Fixture fx;
    const Index a = addOpenBoxComponent(fx.model_layer, "OpenA");
    const Index b = addClosedBoxComponent(fx.model_layer, "B", 0.5, 0.5, 0.5, 1.0);

    fx.selectA(a);
    fx.selectB(b);
    fx.setOp(0); // 并集（任意 op，闭合预检在 op 之前）
    const std::string text = fx.invokeText();

    REQUIRE(text.find("对象 A 不是闭合") != std::string::npos);

    // 预检拒绝 → 不生成新模型，且原网格未被修改
    REQUIRE(resultMeshOf(fx.model_layer, "OpenA_并集_B") == nullptr);
    const ComponentData* after = fx.model_layer.findComponent(a);
    REQUIRE(after != nullptr);
    REQUIRE(after->mesh != nullptr);
    REQUIRE(faceCount(*after->mesh) == 10);
}
