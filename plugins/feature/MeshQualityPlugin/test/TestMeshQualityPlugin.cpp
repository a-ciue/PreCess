#include "ArgObject.h"
#include "ComponentData.h"
#include "EventBus.h"
#include "FeatureEvents.h"
#include "FeatureSystem.h"
#include "MeshData.h"
#include "MeshQualityHandler.h"
#include "ModelLayer.h"
#include "Selection.h"
#include "SessionQuery.h"
#include "SolidCellRecognition.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <vtkCellType.h>
#include <vtkDoubleArray.h>
#include <vtkMeshQuality.h>
#include <vtkNew.h>

#include <algorithm>
#include <any>
#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <vector>

using namespace systems;
using namespace systems::feature;

namespace {

// 与质量指标选择器的公开选项顺序对应，测试直接验证该参数契约。
constexpr int kScaledJacobian = 0;
constexpr int kEquiangleSkew = 1;
constexpr int kEdgeRatio = 2;
constexpr int kMinAngle = 3;
constexpr int kMaxAngle = 4;
constexpr int kWarpage = 5;
constexpr int kTetCollapse = 6;
constexpr std::array kMetricKeys { "scaled_jacobian", "equiangle_skew", "edge_ratio", "min_angle", "max_angle", "warpage", "tet_collapse" };

/**
 * @brief 创建同时包含理想三角形和理想四面体的单组件模型
 */
Index addQualityTestComponent(ModelLayer& model_layer)
{
    auto mesh = std::make_unique<MeshData>();
    mesh->init();

    const double sqrt_three = std::sqrt(3.0);
    mesh->vertex_positions_ = {
        { 0.0, 0.0, 0.0 },
        { 1.0, 0.0, 0.0 },
        { 0.5, sqrt_three / 2.0, 0.0 },
        { 0.5, sqrt_three / 6.0, std::sqrt(2.0 / 3.0) },
    };
    mesh->face_vertices_ = { 0, 1, 2 };
    mesh->face_vertices_offset_ = { 0, 3 };
    mesh->solid_types_ = { 10 }; // VTK_TETRA
    mesh->solid_vertices_ = { 0, 1, 2, 3 };
    mesh->solid_vertices_offset_ = { 0, 4 };

    auto component = std::make_unique<ComponentData>();
    component->name = "QualityComponent";
    component->mesh = std::move(mesh);
    ComponentDatas components;
    components.push_back(std::move(component));

    const Index model_id = model_layer.addModel("QualityModel", std::move(components));
    return model_layer.modelById(model_id)->componentIds().front();
}

/**
 * @brief 创建测试使用的 Feature 元数据
 */
HandlerMetaData qualityMetaData()
{
    HandlerMetaData metadata;
    metadata.name = "MeshQuality";
    metadata.display_name = "网格质量";
    return metadata;
}

/**
 * @brief 创建只包含一个目标 Component 的选择器参数
 */
std::shared_ptr<Selection> makeComponentSelection(Index component_id)
{
    auto selection = std::make_shared<Selection>();
    selection->type = ElementEnum::Component;
    selection->ids = { component_id };
    return selection;
}

/** @brief 装配真实功能系统，使用自持网格验证插件的输入、输出及退出清理。 */
struct QualityFixture {
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem feature_system { model_layer, bus };
    Index component_id { -1 };

    /** @brief 将测试网格安装到组件并通过选择器指定计算目标。 */
    explicit QualityFixture(std::unique_ptr<MeshData> mesh)
    {
        auto component = std::make_unique<ComponentData>();
        component->mesh = std::move(mesh);
        ComponentDatas components;
        components.push_back(std::move(component));
        const Index model_id = model_layer.addModel("QualityRegression", std::move(components));
        component_id = model_layer.modelById(model_id)->componentIds().front();
        FeatureSystem::SystemHandlerPtr handler { new MeshQualityHandler };
        REQUIRE(feature_system.registerHandler(qualityMetaData(), std::move(handler)));
        REQUIRE(feature_system.setParameter("MeshQuality", 0,
            core::ArgObject::create<ArgTypeEnum::Selector>(makeComponentSelection(component_id))));
    }

    /** @brief 按指定指标执行真实插件并提取反馈文本。 */
    std::string execute(int metric)
    {
        REQUIRE(feature_system.setParameter("MeshQuality", 1,
            core::ArgObject::create<ArgTypeEnum::Combo>(metric)));
        return std::any_cast<std::string>(feature_system.invoke("MeshQuality"));
    }

    /** @brief 只读查询插件写入后的网格属性。 */
    const MeshData& mesh() const
    {
        return *model_layer.findComponent(component_id)->mesh;
    }
};

/** @brief 创建单位立方体网格，供精度、异常单元和属性生命周期回归复用。 */
std::unique_ptr<MeshData> makeHexMesh()
{
    auto mesh = std::make_unique<MeshData>();
    mesh->init();
    mesh->vertex_positions_ = {
        { 0, 0, 0 },
        { 1, 0, 0 },
        { 1, 1, 0 },
        { 0, 1, 0 },
        { 0, 0, 1 },
        { 1, 0, 1 },
        { 1, 1, 1 },
        { 0, 1, 1 },
    };
    mesh->solid_types_ = { VTK_HEXAHEDRON };
    mesh->solid_vertices_ = { 0, 1, 2, 3, 4, 5, 6, 7 };
    mesh->solid_vertices_offset_ = { 0, 8 };
    return mesh;
}

/** @brief 用同一组坐标构造原生标准单元或其折叠六面体编码，供独立对照计算。 */
std::unique_ptr<MeshData> makeCollapsedMesh(int type, bool folded)
{
    auto mesh = makeHexMesh();
    std::vector<Index> ids;
    if (type == VTK_WEDGE) {
        const double height = std::sqrt(3.0) / 2.0;
        mesh->vertex_positions_ = {
            { 0, 0, 0 },
            { 1, 0, 0 },
            { 0.5, height, 0 },
            { 0, 0, 1 },
            { 1, 0, 1 },
            { 0.5, height, 1 },
        };
        ids = { 0, 1, 1, 2, 3, 4, 4, 5 };
    } else if (type == VTK_PYRAMID) {
        mesh->vertex_positions_.resize(5);
        mesh->vertex_positions_[4] = { 0.5, 0.5, std::sqrt(0.5) };
        ids = { 0, 1, 2, 3, 4, 4, 4, 4 };
    } else {
        mesh->vertex_positions_ = {
            { 0, 0, 0 },
            { 1, 0, 0 },
            { 0.5, std::sqrt(3.0) / 2.0, 0 },
            { 0.5, std::sqrt(3.0) / 6.0, std::sqrt(2.0 / 3.0) },
        };
        ids = { 0, 1, 2, 2, 3, 3, 3, 3 };
    }
    if (folded) {
        mesh->solid_vertices_ = ids;
    } else {
        mesh->solid_types_ = { static_cast<unsigned char>(type) };
        mesh->solid_vertices_.resize(mesh->vertex_positions_.size());
        for (std::size_t i = 0; i < mesh->solid_vertices_.size(); ++i)
            mesh->solid_vertices_[i] = static_cast<Index>(i);
    }
    mesh->solid_vertices_offset_ = { 0, static_cast<Index>(mesh->solid_vertices_.size()) };
    return mesh;
}

}

TEST_CASE("MeshQuality computes scalar attributes", "[MeshQualityPlugin]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem feature_system(model_layer, bus);
    const Index component_id = addQualityTestComponent(model_layer);
    std::vector<std::string> display_requests;
    std::vector<Index> display_component_ids;
    auto display_subscription = bus.subscribe<ScalarAttributeDisplayRequestedEvent>(
        [&display_requests, &display_component_ids](const ScalarAttributeDisplayRequestedEvent& event) {
            display_requests.push_back(event.attribute_name);
            display_component_ids.push_back(event.component_id);
        });
    REQUIRE(static_cast<bool>(display_subscription));

    FeatureSystem::SystemHandlerPtr handler { new MeshQualityHandler };
    REQUIRE(feature_system.registerHandler(qualityMetaData(), std::move(handler)));
    REQUIRE(feature_system.setParameter(
        "MeshQuality", 0,
        core::ArgObject::create<ArgTypeEnum::Selector>(makeComponentSelection(component_id))));

    const std::any execution_result = feature_system.invoke("MeshQuality");
    REQUIRE(execution_result.type() == typeid(std::string));
    REQUIRE(std::any_cast<const std::string&>(execution_result).find("Scaled Jacobian") != std::string::npos);
    REQUIRE(display_requests == std::vector<std::string> { "s_mesh_quality_scaled_jacobian_1" });
    REQUIRE(display_component_ids == std::vector<Index> { component_id });

    const ComponentData* component = model_layer.findComponent(component_id);
    REQUIRE(component != nullptr);
    REQUIRE(component->mesh != nullptr);

    const auto& face_quality = component->mesh->face_attributes_.at("f_mesh_quality_scaled_jacobian_1");
    const auto& solid_quality = component->mesh->solid_attributes_.at("s_mesh_quality_scaled_jacobian_1");
    REQUIRE(face_quality.size() == 1);
    REQUIRE(solid_quality.size() == 1);
    REQUIRE(face_quality.front() == Catch::Approx(1.0));
    REQUIRE(solid_quality.front() == Catch::Approx(1.0));

    // 操作进行期间保留不同指标，方便用户在属性列表中比较。
    REQUIRE(feature_system.setParameter(
        "MeshQuality", 1, core::ArgObject::create<ArgTypeEnum::Combo>(1)));
    const std::any skew_result = feature_system.invoke("MeshQuality");
    REQUIRE(skew_result.type() == typeid(std::string));
    REQUIRE(display_requests.back() == "s_mesh_quality_equiangle_skew_1");
    REQUIRE(display_requests.size() == 2);
    REQUIRE(display_component_ids.back() == component_id);
    REQUIRE(component->mesh->face_attributes_.count("f_mesh_quality_scaled_jacobian_1") == 1);
    REQUIRE(component->mesh->solid_attributes_.count("s_mesh_quality_scaled_jacobian_1") == 1);
    REQUIRE(component->mesh->face_attributes_.count("f_mesh_quality_equiangle_skew_1") == 1);
    REQUIRE(component->mesh->solid_attributes_.count("s_mesh_quality_equiangle_skew_1") == 1);

    // 功能退出（活动操作切走）时删除本次写入的全部质量属性
    REQUIRE(feature_system.setFeatureActive("MeshQuality"));
    REQUIRE(feature_system.setFeatureActive(""));
    REQUIRE(component->mesh->face_attributes_.count("f_mesh_quality_scaled_jacobian_1") == 0);
    REQUIRE(component->mesh->solid_attributes_.count("s_mesh_quality_scaled_jacobian_1") == 0);
    REQUIRE(component->mesh->face_attributes_.count("f_mesh_quality_equiangle_skew_1") == 0);
    REQUIRE(component->mesh->solid_attributes_.count("s_mesh_quality_equiangle_skew_1") == 0);
}

TEST_CASE("MeshQuality exposes component selection and metric parameters", "[MeshQualityPlugin]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem feature_system(model_layer, bus);

    FeatureSystem::SystemHandlerPtr handler { new MeshQualityHandler };
    REQUIRE(feature_system.registerHandler(qualityMetaData(), std::move(handler)));

    const auto infos = feature_system.getFeatureInfos();
    REQUIRE(infos.size() == 1);
    REQUIRE(infos.front()->arg_types.size() == 2);
    REQUIRE(infos.front()->arg_types.front().type == ArgTypeEnum::Selector);
    REQUIRE(infos.front()->arg_types.front().content == "Component");
    REQUIRE(infos.front()->arg_types[1].type == ArgTypeEnum::Combo);
    REQUIRE(infos.front()->arg_types[1].content.find("Tet Collapse") != std::string::npos);
}

TEST_CASE("MeshQuality requires exactly one selected component", "[MeshQualityPlugin]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem feature_system(model_layer, bus);
    const Index component_id = addQualityTestComponent(model_layer);

    FeatureSystem::SystemHandlerPtr handler { new MeshQualityHandler };
    REQUIRE(feature_system.registerHandler(qualityMetaData(), std::move(handler)));

    const std::any empty_result = feature_system.invoke("MeshQuality");
    REQUIRE(std::any_cast<const std::string&>(empty_result).find("请选择一个 Component") != std::string::npos);

    auto multiple_selection = makeComponentSelection(component_id);
    multiple_selection->ids.push_back(component_id);
    REQUIRE(feature_system.setParameter(
        "MeshQuality", 0,
        core::ArgObject::create<ArgTypeEnum::Selector>(std::move(multiple_selection))));

    const std::any multiple_result = feature_system.invoke("MeshQuality");
    REQUIRE(std::any_cast<const std::string&>(multiple_result).find("只能选择一个 Component") != std::string::npos);
}

TEST_CASE("MeshQuality isolates unavailable values and preserves cell indices", "[MeshQualityPlugin]")
{
    auto mesh = makeHexMesh();
    mesh->solid_types_.push_back(VTK_HEXAHEDRON);
    mesh->solid_vertices_.insert(mesh->solid_vertices_.end(), { 0, 1, 2, 3, 4, 5, 4, 5 });
    mesh->solid_vertices_offset_.push_back(16);
    QualityFixture fixture(std::move(mesh));

    for (int metric = 0; metric < 3; ++metric) {
        INFO("metric=" << metric);
        const auto output = fixture.execute(metric);
        CHECK(output.find("有效 1") != std::string::npos);
        CHECK(output.find("不可用 1") != std::string::npos);
        CHECK(output.find("e+30") == std::string::npos);
    }
    for (const auto& [name, values] : fixture.mesh().solid_attributes_) {
        INFO(name);
        REQUIRE(values.size() == 2);
        CHECK(std::isfinite(values[0]));
        CHECK(std::isnan(values[1]));
    }

    // NaN 保留单元位置，VTK 的正常值域应仅由可用结果决定。
    vtkNew<vtkDoubleArray> array;
    for (double value : fixture.mesh().solid_attributes_.at("s_mesh_quality_scaled_jacobian_1"))
        array->InsertNextValue(value);
    double range[2];
    array->GetRange(range);
    CHECK(range[0] == Catch::Approx(1.0));
    CHECK(range[1] == Catch::Approx(1.0));
}

TEST_CASE("MeshQuality reports entirely unavailable cells without normal statistics", "[MeshQualityPlugin]")
{
    auto mesh = makeHexMesh();
    mesh->solid_vertices_[2] = 1;
    QualityFixture fixture(std::move(mesh));
    const auto output = fixture.execute(kScaledJacobian);
    CHECK(output.find("有效 0") != std::string::npos);
    CHECK(output.find("不可用 1") != std::string::npos);
    CHECK(output.find("平均值") == std::string::npos);
    CHECK(fixture.mesh().solid_attributes_.empty());
}

TEST_CASE("MeshQuality preserves double precision under large coordinate translations", "[MeshQualityPlugin]")
{
    auto mesh = makeHexMesh();
    for (auto& point : mesh->vertex_positions_) {
        for (auto& coordinate : point)
            coordinate += 1e9;
    }
    QualityFixture fixture(std::move(mesh));
    fixture.execute(kScaledJacobian);
    CHECK(fixture.mesh().solid_attributes_.at("s_mesh_quality_scaled_jacobian_1")[0] == Catch::Approx(1.0));
    fixture.execute(kEquiangleSkew);
    CHECK(fixture.mesh().solid_attributes_.at("s_mesh_quality_equiangle_skew_1")[0] == Catch::Approx(0.0).margin(1e-12));
    fixture.execute(kEdgeRatio);
    CHECK(fixture.mesh().solid_attributes_.at("s_mesh_quality_edge_ratio_1")[0] == Catch::Approx(1.0));
}

TEST_CASE("MeshQuality excludes finite Verdict sentinels without repeated vertices", "[MeshQualityPlugin]")
{
    auto mesh = makeHexMesh();
    mesh->vertex_positions_[1] = { 1e-10, 0, 0 };
    QualityFixture fixture(std::move(mesh));
    const auto output = fixture.execute(kScaledJacobian);
    CHECK(output.find("不可用 1") != std::string::npos);
    CHECK(fixture.mesh().solid_attributes_.empty());
}

TEST_CASE("MeshQuality collapse ratio uses each opposite face longest edge", "[MeshQualityPlugin]")
{
    auto mesh = std::make_unique<MeshData>();
    mesh->init();
    mesh->vertex_positions_ = { { 0, 0, 0 }, { 8, 0, 0 }, { 1, 2, 0 }, { -5, 8, 7 } };
    mesh->solid_types_ = { VTK_TETRA };
    mesh->solid_vertices_ = { 0, 1, 2, 3 };
    mesh->solid_vertices_offset_ = { 0, 4 };
    QualityFixture fixture(std::move(mesh));
    fixture.execute(kTetCollapse);
    // 四个面的高/该面最长边的最小值，独立按几何公式求得。
    CHECK(fixture.mesh().solid_attributes_.at("s_mesh_quality_tet_collapse_1")[0]
        == Catch::Approx(0.07842675750005629).epsilon(1e-12));
}

TEST_CASE("MeshQuality preserves preexisting attributes and reuses its own names", "[MeshQualityPlugin]")
{
    auto mesh = makeHexMesh();
    mesh->face_vertices_ = { 0, 1, 2, 3 };
    mesh->face_vertices_offset_ = { 0, 4 };
    mesh->face_attributes_["f_mesh_quality_scaled_jacobian_1"] = { 42.0 };
    mesh->solid_attributes_["s_mesh_quality_scaled_jacobian_1"] = { 43.0 };
    QualityFixture fixture(std::move(mesh));
    REQUIRE(fixture.feature_system.setFeatureActive("MeshQuality"));
    fixture.execute(kScaledJacobian);
    fixture.execute(kScaledJacobian);
    CHECK(fixture.mesh().face_attributes_.at("f_mesh_quality_scaled_jacobian_1")[0] == 42.0);
    CHECK(fixture.mesh().solid_attributes_.at("s_mesh_quality_scaled_jacobian_1")[0] == 43.0);
    CHECK(fixture.mesh().face_attributes_.size() == 2);
    CHECK(fixture.mesh().solid_attributes_.size() == 2);
    REQUIRE(fixture.feature_system.setFeatureActive(""));
    CHECK(fixture.mesh().face_attributes_.size() == 1);
    CHECK(fixture.mesh().solid_attributes_.size() == 1);
    CHECK(fixture.mesh().face_attributes_.at("f_mesh_quality_scaled_jacobian_1")[0] == 42.0);
    CHECK(fixture.mesh().solid_attributes_.at("s_mesh_quality_scaled_jacobian_1")[0] == 43.0);
}

TEST_CASE("MeshQuality rejects invalid metric indices", "[MeshQualityPlugin]")
{
    QualityFixture fixture(makeHexMesh());
    for (int metric : { -1, 7, 100 }) {
        CHECK(fixture.execute(metric).find("无效的质量指标") != std::string::npos);
        CHECK(fixture.mesh().solid_attributes_.empty());
    }
    // 脚本宿主也能提交错误的参数类型，不能因此静默计算另一个指标。
    REQUIRE(fixture.feature_system.setParameter("MeshQuality", 1,
        core::ArgObject::create<ArgTypeEnum::Float>(2.0)));
    CHECK(std::any_cast<std::string>(fixture.feature_system.invoke("MeshQuality")).find("无效的质量指标") != std::string::npos);
    CHECK(fixture.mesh().solid_attributes_.empty());
}

TEST_CASE("MeshQuality covers supported linear cell and metric combinations", "[MeshQualityPlugin]")
{
    const double height = std::sqrt(3.0) / 2.0;
    const int cell_type = GENERATE(VTK_TRIANGLE, VTK_QUAD, VTK_TETRA, VTK_PYRAMID, VTK_WEDGE, VTK_HEXAHEDRON);
    const int metric = GENERATE(kScaledJacobian, kEquiangleSkew, kEdgeRatio, kMinAngle, kMaxAngle, kWarpage, kTetCollapse);
    INFO("cell_type=" << cell_type << ", metric=" << metric);
    auto mesh = makeHexMesh();
    switch (cell_type) {
    case VTK_TRIANGLE:
    case VTK_TETRA:
        mesh->vertex_positions_ = {
            { 0, 0, 0 },
            { 1, 0, 0 },
            { 0.5, height, 0 },
            { 0.5, height / 3.0, std::sqrt(2.0 / 3.0) },
        };
        break;
    case VTK_PYRAMID:
        mesh->vertex_positions_.resize(5);
        mesh->vertex_positions_[4] = { 0.5, 0.5, std::sqrt(0.5) };
        break;
    case VTK_WEDGE:
        // 与三棱柱形函数的参数坐标保持同一正方向。
        mesh->vertex_positions_ = {
            { 0, 0, 0 },
            { 1, 0, 0 },
            { 0.5, height, 0 },
            { 0, 0, 1 },
            { 1, 0, 1 },
            { 0.5, height, 1 },
        };
        break;
    }
    const bool is_face = cell_type == VTK_TRIANGLE || cell_type == VTK_QUAD;
    if (is_face) {
        mesh->solid_types_.clear();
        mesh->solid_vertices_.clear();
        mesh->solid_vertices_offset_ = { 0 };
        mesh->face_vertices_ = cell_type == VTK_TRIANGLE
            ? std::vector<Index> { 0, 1, 2 }
            : std::vector<Index> { 0, 1, 2, 3 };
        mesh->face_vertices_offset_ = { 0, static_cast<Index>(mesh->face_vertices_.size()) };
    } else {
        mesh->solid_types_ = { static_cast<unsigned char>(cell_type) };
        mesh->solid_vertices_.resize(mesh->vertex_positions_.size());
        for (std::size_t i = 0; i < mesh->solid_vertices_.size(); ++i)
            mesh->solid_vertices_[i] = static_cast<Index>(i);
        mesh->solid_vertices_offset_ = { 0, static_cast<Index>(mesh->solid_vertices_.size()) };
    }
    QualityFixture fixture(std::move(mesh));
    const auto output = fixture.execute(metric);
    const bool supported = is_face ? metric != kTetCollapse && (metric != kWarpage || cell_type == VTK_QUAD)
        : metric <= kEdgeRatio     ? metric != kEdgeRatio || cell_type != VTK_PYRAMID
                                   : metric == kTetCollapse && cell_type == VTK_TETRA;
    const auto& attributes = is_face ? fixture.mesh().face_attributes_ : fixture.mesh().solid_attributes_;
    if (!supported) {
        CHECK(output.find("没有可计算的单元") != std::string::npos);
        CHECK(attributes.empty());
        return;
    }
    REQUIRE(attributes.size() == 1);
    const auto& values = attributes.at(std::string(is_face ? "f_" : "s_") + "mesh_quality_" + std::string(kMetricKeys[metric]) + "_1");
    REQUIRE(values.size() == 1);
    double expected = 1.0;
    if (metric == kEquiangleSkew)
        expected = 0.0;
    if (metric == kMinAngle || metric == kMaxAngle)
        expected = cell_type == VTK_TRIANGLE ? 60.0 : 90.0;
    if (metric == kTetCollapse)
        expected = std::sqrt(2.0 / 3.0);
    CHECK(values[0] == Catch::Approx(expected).margin(1e-12));
}

TEST_CASE("MeshQuality keeps signed inverted and zero quality values", "[MeshQualityPlugin]")
{
    auto mesh = std::make_unique<MeshData>();
    mesh->init();
    mesh->vertex_positions_ = { { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, -1 } };
    mesh->solid_types_ = { VTK_TETRA };
    mesh->solid_vertices_ = { 0, 1, 2, 3 };
    mesh->solid_vertices_offset_ = { 0, 4 };
    SECTION("inverted tetrahedron")
    {
        QualityFixture fixture(std::move(mesh));
        fixture.execute(kScaledJacobian);
        CHECK(fixture.mesh().solid_attributes_.at("s_mesh_quality_scaled_jacobian_1")[0] < 0.0);
        fixture.execute(kTetCollapse);
        CHECK(fixture.mesh().solid_attributes_.at("s_mesh_quality_tet_collapse_1")[0] < 0.0);
    }
    SECTION("flat tetrahedron with distinct vertices")
    {
        mesh->vertex_positions_[3] = { 1, 1, 0 };
        QualityFixture fixture(std::move(mesh));
        fixture.execute(kScaledJacobian);
        CHECK(fixture.mesh().solid_attributes_.at("s_mesh_quality_scaled_jacobian_1")[0] == 0.0);
        fixture.execute(kTetCollapse);
        CHECK(fixture.mesh().solid_attributes_.at("s_mesh_quality_tet_collapse_1")[0] == 0.0);
    }
}

TEST_CASE("MeshQuality rejects malformed connectivity before calling VTK", "[MeshQualityPlugin]")
{
    auto mesh = makeHexMesh();
    std::string expected_error = "体单元顶点偏移与连通性不一致";
    SECTION("missing solid offsets") { mesh->solid_vertices_offset_.clear(); }
    SECTION("nonzero first offset")
    {
        mesh->solid_vertices_.insert(mesh->solid_vertices_.begin(), 7);
        mesh->solid_vertices_offset_ = { 1, 9 };
    }
    SECTION("unreferenced trailing indices") { mesh->solid_vertices_.push_back(0); }
    SECTION("unordered offsets") { mesh->solid_vertices_offset_ = { 0, 8, 4, 8 }; }
    SECTION("type count mismatch")
    {
        mesh->solid_types_.clear();
        expected_error = "体单元类型与顶点偏移数量不一致";
    }
    SECTION("wrong corner count")
    {
        mesh->solid_vertices_.pop_back();
        mesh->solid_vertices_offset_.back() = 7;
        expected_error = "体单元顶点数量与单元类型不一致";
    }
    SECTION("invalid local point")
    {
        mesh->solid_vertices_[0] = 8;
        expected_error = "体单元包含无效的点索引";
    }
    QualityFixture fixture(std::move(mesh));
    const auto output = fixture.execute(kScaledJacobian);
    CHECK(output.find(expected_error) != std::string::npos);
    CHECK(fixture.mesh().solid_attributes_.empty());
}

TEST_CASE("MeshQuality marks coincident or nonfinite vertices unavailable for every face metric", "[MeshQualityPlugin]")
{
    auto mesh = makeHexMesh();
    mesh->solid_types_.clear();
    mesh->solid_vertices_.clear();
    mesh->solid_vertices_offset_ = { 0 };
    mesh->face_vertices_ = { 0, 1, 2, 3, 4, 5, 6, 7 };
    mesh->face_vertices_offset_ = { 0, 4, 8 };
    SECTION("coincident vertices with different IDs") { mesh->vertex_positions_[6] = mesh->vertex_positions_[5]; }
    SECTION("infinite coordinate") { mesh->vertex_positions_[6][0] = std::numeric_limits<double>::infinity(); }
    SECTION("NaN coordinate") { mesh->vertex_positions_[6][0] = std::numeric_limits<double>::quiet_NaN(); }
    QualityFixture fixture(std::move(mesh));
    for (int metric = 0; metric < 6; ++metric) {
        const auto output = fixture.execute(metric);
        CHECK(output.find("有效 1") != std::string::npos);
        CHECK(output.find("不可用 1") != std::string::npos);
    }
    for (const auto& [name, values] : fixture.mesh().face_attributes_) {
        INFO(name);
        REQUIRE(values.size() == 2);
        CHECK(std::isfinite(values[0]));
        CHECK(std::isnan(values[1]));
    }
}

TEST_CASE("MeshQuality computes signed wedge scaled Jacobians in shape function order", "[MeshQualityPlugin][WedgeJacobian]")
{
    const bool folded = GENERATE(false, true);
    const bool inverted = GENERATE(false, true);
    const bool right_triangle = GENERATE(false, true);
    INFO("folded=" << folded << ", inverted=" << inverted << ", right_triangle=" << right_triangle);
    auto mesh = makeCollapsedMesh(VTK_WEDGE, folded);
    if (right_triangle) {
        mesh->vertex_positions_ = {
            { 0, 0, 0 },
            { 2, 0, 0 },
            { 0, 1, 0 },
            { 0, 0, 1 },
            { 2, 0, 1 },
            { 0, 1, 1 },
        };
    }
    // 交换对应的两对角点，反转映射方向；折叠编码仍描述同一个三棱柱。
    if (inverted) {
        std::swap(mesh->vertex_positions_[1], mesh->vertex_positions_[2]);
        std::swap(mesh->vertex_positions_[4], mesh->vertex_positions_[5]);
    }
    const auto original_ids = mesh->solid_vertices_;
    const auto original_types = mesh->solid_types_;
    const auto original_offsets = mesh->solid_vertices_offset_;
    const auto original_positions = mesh->vertex_positions_;
    QualityFixture fixture(std::move(mesh));
    const auto output = fixture.execute(kScaledJacobian);
    const auto& values = fixture.mesh().solid_attributes_.at("s_mesh_quality_scaled_jacobian_1");
    REQUIRE(values.size() == 1);
    // 直棱柱的六角点质量为底角正弦乘 2/sqrt(3)，取有符号最小值。
    // 直角三角形底的最小正值与倒置后的最小负值不互为相反数，防止以取绝对值或反号修补。
    const double expected = right_triangle ? (inverted ? -2.0 / std::sqrt(3.0) : 2.0 / std::sqrt(15.0))
                                           : (inverted ? -1.0 : 1.0);
    CHECK(values[0] == Catch::Approx(expected).margin(1e-12));
    CHECK(output.find(folded ? "折叠转换 1" : "折叠转换 0") != std::string::npos);
    CHECK(output.find("三棱柱 1") != std::string::npos);
    CHECK(fixture.mesh().solid_attributes_.size() == 1);
    CHECK(fixture.mesh().solid_vertices_ == original_ids);
    CHECK(fixture.mesh().solid_types_ == original_types);
    CHECK(fixture.mesh().solid_vertices_offset_ == original_offsets);
    CHECK(fixture.mesh().vertex_positions_ == original_positions);
}

TEST_CASE("MeshQuality preserves orientation when a wedge collapses to a tetrahedron", "[MeshQualityPlugin][WedgeJacobian]")
{
    const bool inverted = GENERATE(false, true);
    auto mesh = makeCollapsedMesh(VTK_TETRA, false);
    mesh->solid_types_ = { VTK_WEDGE };
    mesh->solid_vertices_ = { 0, 1, 2, 3, 3, 3 };
    mesh->solid_vertices_offset_ = { 0, 6 };
    if (inverted)
        std::swap(mesh->vertex_positions_[1], mesh->vertex_positions_[2]);
    QualityFixture fixture(std::move(mesh));
    const auto output = fixture.execute(kScaledJacobian);
    CHECK(fixture.mesh().solid_attributes_.at("s_mesh_quality_scaled_jacobian_1")[0]
        == Catch::Approx(inverted ? -1.0 : 1.0).margin(1e-12));
    CHECK(output.find("折叠转换 1") != std::string::npos);
    CHECK(output.find("四面体 1") != std::string::npos);
}

TEST_CASE("MeshQuality evaluates folded cells as their actual standard types", "[MeshQualityPlugin]")
{
    const int type = GENERATE(VTK_TETRA, VTK_PYRAMID, VTK_WEDGE);
    const int metric = GENERATE(kScaledJacobian, kEquiangleSkew, kEdgeRatio, kTetCollapse);
    INFO("type=" << type << ", metric=" << metric);
    auto source = makeCollapsedMesh(type, true);
    const auto original_vertices = source->solid_vertices_;
    const auto original_types = source->solid_types_;
    const auto original_offsets = source->solid_vertices_offset_;
    const auto original_positions = source->vertex_positions_;
    QualityFixture folded(std::move(source));
    QualityFixture native(makeCollapsedMesh(type, false));
    const auto output = folded.execute(metric);
    native.execute(metric);
    const std::string key = "s_mesh_quality_" + std::string(kMetricKeys[metric]) + "_1";
    if (native.mesh().solid_attributes_.empty()) {
        CHECK(folded.mesh().solid_attributes_.empty());
    } else {
        REQUIRE(folded.mesh().solid_attributes_.size() == 1);
        CHECK(folded.mesh().solid_attributes_.at(key)[0]
            == Catch::Approx(native.mesh().solid_attributes_.at(key)[0]).margin(1e-12));
    }
    CHECK(output.find("折叠转换 1") != std::string::npos);
    CHECK(folded.mesh().solid_vertices_ == original_vertices);
    CHECK(folded.mesh().solid_types_ == original_types);
    CHECK(folded.mesh().solid_vertices_offset_ == original_offsets);
    CHECK(folded.mesh().vertex_positions_ == original_positions);
}

TEST_CASE("MeshQuality preserves original IDs across recognized and unavailable mixed cells", "[MeshQualityPlugin]")
{
    auto mesh = makeCollapsedMesh(VTK_PYRAMID, true);
    const auto tetra = makeCollapsedMesh(VTK_TETRA, true);
    // 同一组五点可同时定义金字塔、未识别折叠单元和四面体；保留原始 0/1/2 顺序。
    mesh->solid_types_ = { VTK_HEXAHEDRON, VTK_HEXAHEDRON, VTK_HEXAHEDRON };
    mesh->solid_vertices_.insert(mesh->solid_vertices_.end(), { 0, 1, 0, 1, 2, 3, 2, 3 });
    mesh->solid_vertices_.insert(mesh->solid_vertices_.end(), tetra->solid_vertices_.begin(), tetra->solid_vertices_.end());
    mesh->solid_vertices_offset_ = { 0, 8, 16, 24 };
    const auto original_ids = mesh->solid_vertices_;
    const auto original_types = mesh->solid_types_;
    const auto original_offsets = mesh->solid_vertices_offset_;
    QualityFixture fixture(std::move(mesh));
    const auto output = fixture.execute(kEdgeRatio);
    const auto& values = fixture.mesh().solid_attributes_.at("s_mesh_quality_edge_ratio_1");
    REQUIRE(values.size() == 3);
    CHECK(std::isnan(values[0])); // 金字塔已识别，但 VTK 不支持该类型的 Edge Ratio。
    CHECK(std::isnan(values[1])); // 四个不同角点的自粘面不是标准四面体。
    CHECK(std::isfinite(values[2]));
    CHECK(output.find("类型未识别 1") != std::string::npos);
    CHECK(output.find("指标不支持 1") != std::string::npos);
    CHECK(fixture.mesh().solid_attributes_.size() == 1);
    CHECK(fixture.mesh().solid_vertices_ == original_ids);
    CHECK(fixture.mesh().solid_types_ == original_types);
    CHECK(fixture.mesh().solid_vertices_offset_ == original_offsets);
}

TEST_CASE("MeshQuality keeps inverted and flat folded tetrahedra signed", "[MeshQualityPlugin]")
{
    auto mesh = makeCollapsedMesh(VTK_TETRA, true);
    SECTION("inverted geometry")
    {
        for (auto& point : mesh->vertex_positions_)
            point[0] = -point[0];
        QualityFixture fixture(std::move(mesh));
        fixture.execute(kScaledJacobian);
        CHECK(fixture.mesh().solid_attributes_.at("s_mesh_quality_scaled_jacobian_1")[0] < 0);
    }
    SECTION("zero height with distinct vertices")
    {
        mesh->vertex_positions_[3][2] = 0;
        QualityFixture fixture(std::move(mesh));
        fixture.execute(kScaledJacobian);
        CHECK(fixture.mesh().solid_attributes_.at("s_mesh_quality_scaled_jacobian_1")[0] == 0);
    }
}

TEST_CASE("MeshQuality preserves imported diagnostic attributes without creating new ones", "[MeshQualityPlugin]")
{
    auto mesh = makeCollapsedMesh(VTK_WEDGE, true);
    mesh->solid_attributes_["s_mesh_quality_recognition_status_1"] = { 99 };
    QualityFixture fixture(std::move(mesh));
    REQUIRE(fixture.feature_system.setFeatureActive("MeshQuality"));
    fixture.execute(kScaledJacobian);
    fixture.execute(kEquiangleSkew);
    CHECK(fixture.mesh().solid_attributes_.size() == 3);
    CHECK(fixture.mesh().solid_attributes_.at("s_mesh_quality_recognition_status_1")[0] == 99);
    CHECK_FALSE(fixture.mesh().solid_attributes_.contains("s_mesh_quality_recognition_status_2_1"));
    REQUIRE(fixture.feature_system.setFeatureActive(""));
    REQUIRE(fixture.mesh().solid_attributes_.size() == 1);
    CHECK(fixture.mesh().solid_attributes_.at("s_mesh_quality_recognition_status_1")[0] == 99);
}

TEST_CASE("MeshQuality reuses folded patterns without sharing node IDs or geometry", "[MeshQualityPlugin]")
{
    const int type = GENERATE(VTK_TETRA, VTK_PYRAMID, VTK_WEDGE);
    const int metric = GENERATE(kScaledJacobian, kEdgeRatio, kTetCollapse);
    auto mesh = makeCollapsedMesh(type, true);
    const auto positions = mesh->vertex_positions_;
    const auto folded_ids = mesh->solid_vertices_;
    mesh->vertex_positions_.clear();
    mesh->solid_types_.clear();
    mesh->solid_vertices_.clear();
    mesh->solid_vertices_offset_ = { 0 };
    std::vector<double> expected_values;
    const std::string key = "s_mesh_quality_" + std::string(kMetricKeys[metric]) + "_1";

    // 相同模式使用不同实际编号、几何尺寸和朝向，防止误复用坐标或质量数值。
    constexpr Index copy_count = 24;
    for (Index copy = 0; copy < copy_count; ++copy) {
        auto native = makeCollapsedMesh(type, false);
        for (auto& point : native->vertex_positions_) {
            point[0] *= copy % 3 == 0 ? -1.0 : 1.0 + copy * 0.01;
            point[1] += point[2] * 0.1;
        }
        const Index base = static_cast<Index>(mesh->vertex_positions_.size());
        const Index count = static_cast<Index>(positions.size());
        for (Index i = 0; i < count; ++i)
            mesh->vertex_positions_.push_back(native->vertex_positions_[copy % 2 ? count - i - 1 : i]);
        for (Index id : folded_ids)
            mesh->solid_vertices_.push_back(base + (copy % 2 ? count - id - 1 : id));
        mesh->solid_types_.push_back(VTK_HEXAHEDRON);
        mesh->solid_vertices_offset_.push_back(static_cast<Index>(mesh->solid_vertices_.size()));
        QualityFixture reference(std::move(native));
        reference.execute(metric);
        const auto value = reference.mesh().solid_attributes_.find(key);
        expected_values.push_back(value == reference.mesh().solid_attributes_.end()
                ? std::numeric_limits<double>::quiet_NaN()
                : value->second[0]);
    }
    // 未识别模式也复用识别结论，但不能影响后续结果槽位和诊断编号。
    for (Index copy = 0; copy < 2; ++copy) {
        mesh->solid_types_.push_back(VTK_HEXAHEDRON);
        mesh->solid_vertices_.insert(mesh->solid_vertices_.end(), { 0, 1, 0, 1, 2, 3, 2, 3 });
        mesh->solid_vertices_offset_.push_back(static_cast<Index>(mesh->solid_vertices_.size()));
        expected_values.push_back(std::numeric_limits<double>::quiet_NaN());
    }
    const auto original_ids = mesh->solid_vertices_;
    const auto original_positions = mesh->vertex_positions_;
    QualityFixture fixture(std::move(mesh));
    const auto output = fixture.execute(metric);
    if (std::any_of(expected_values.begin(), expected_values.end(), [](double value) { return std::isfinite(value); })) {
        REQUIRE(fixture.mesh().solid_attributes_.size() == 1);
        const auto& values = fixture.mesh().solid_attributes_.at(key);
        REQUIRE(values.size() == expected_values.size());
        for (std::size_t i = 0; i < values.size(); ++i) {
            INFO("cell=" << i);
            if (std::isfinite(expected_values[i]))
                CHECK(values[i] == Catch::Approx(expected_values[i]).margin(1e-12));
            else
                CHECK(std::isnan(values[i]));
        }
    } else {
        CHECK(fixture.mesh().solid_attributes_.empty());
    }
    CHECK(output.find("折叠转换 24") != std::string::npos);
    CHECK(output.find("类型未识别 2") != std::string::npos);
    CHECK(fixture.mesh().solid_vertices_ == original_ids);
    CHECK(fixture.mesh().vertex_positions_ == original_positions);
}

TEST_CASE("MeshQuality exposes only a usable selected metric", "[MeshQualityPlugin][QualityOutput]")
{
    auto mesh = makeCollapsedMesh(VTK_TETRA, false);
    bool usable = false;
    SECTION("supported tetrahedron") { usable = true; }
    SECTION("all numerical results unavailable") { mesh->vertex_positions_[0][0] = std::numeric_limits<double>::infinity(); }
    SECTION("unsupported hexahedra") { mesh = makeHexMesh(); }
    SECTION("unrecognized folded cell")
    {
        mesh = makeHexMesh();
        mesh->solid_vertices_[2] = 1;
    }
    QualityFixture fixture(std::move(mesh));
    std::vector<std::string> requests;
    auto subscription = fixture.bus.subscribe<ScalarAttributeDisplayRequestedEvent>(
        [&](const auto& event) { requests.push_back(event.attribute_name); });
    const auto output = fixture.execute(kTetCollapse);
    const auto infos = session::SessionQuery(fixture.model_layer).componentAttributeInfos(fixture.component_id);
    if (usable) {
        REQUIRE(fixture.mesh().solid_attributes_.size() == 1);
        REQUIRE(infos.size() == 1);
        CHECK(infos.front().name == "s_mesh_quality_tet_collapse_1");
        CHECK(requests == std::vector<std::string> { "s_mesh_quality_tet_collapse_1" });
    } else {
        CHECK(fixture.mesh().solid_attributes_.empty());
        CHECK(infos.empty());
        CHECK(requests.empty());
        CHECK(output.find("有效 0") != std::string::npos);
        CHECK(output.find("未生成质量属性") != std::string::npos);
    }
}

TEST_CASE("MeshQuality removes only stale owned results of the selected metric", "[MeshQualityPlugin][QualityOutput]")
{
    auto mesh = makeCollapsedMesh(VTK_TETRA, false);
    mesh->solid_attributes_["s_mesh_quality_tet_collapse_1"] = { 99.0 };
    QualityFixture fixture(std::move(mesh));
    REQUIRE(fixture.feature_system.setFeatureActive("MeshQuality"));
    fixture.execute(kScaledJacobian);
    fixture.execute(kTetCollapse);
    REQUIRE(fixture.mesh().solid_attributes_.contains("s_mesh_quality_tet_collapse_2_1"));
    auto op = fixture.model_layer.getComponentOperator(fixture.component_id);
    REQUIRE(op);
    auto& editable = op->editableMesh(MeshEditKind::Topology);
    SECTION("invalid coordinates") { editable.vertex_positions_[0][0] = std::numeric_limits<double>::infinity(); }
    SECTION("unsupported cell type") { editable.solid_types_[0] = VTK_VOXEL; }
    SECTION("invalid connectivity")
    {
        editable.solid_vertices_.pop_back();
        editable.solid_vertices_offset_.back() = 3;
    }
    fixture.model_layer.flushNotifications();
    std::vector<std::string> requests;
    auto subscription = fixture.bus.subscribe<ScalarAttributeDisplayRequestedEvent>(
        [&](const auto& event) { requests.push_back(event.attribute_name); });
    fixture.execute(kTetCollapse);
    CHECK_FALSE(fixture.mesh().solid_attributes_.contains("s_mesh_quality_tet_collapse_2_1"));
    CHECK(fixture.mesh().solid_attributes_.at("s_mesh_quality_tet_collapse_1") == std::vector<double> { 99.0 });
    CHECK(fixture.mesh().solid_attributes_.contains("s_mesh_quality_scaled_jacobian_1"));
    CHECK(requests.empty());
    REQUIRE(fixture.feature_system.setFeatureActive(""));
    REQUIRE(fixture.mesh().solid_attributes_.size() == 1);
    CHECK(fixture.mesh().solid_attributes_.at("s_mesh_quality_tet_collapse_1") == std::vector<double> { 99.0 });
}

TEST_CASE("MeshQuality displays usable faces when the solid metric is unavailable", "[MeshQualityPlugin][QualityOutput]")
{
    auto mesh = makeCollapsedMesh(VTK_PYRAMID, false);
    mesh->face_vertices_ = { 0, 1, 2, 3 };
    mesh->face_vertices_offset_ = { 0, 4 };
    QualityFixture fixture(std::move(mesh));
    std::vector<std::string> requests;
    auto subscription = fixture.bus.subscribe<ScalarAttributeDisplayRequestedEvent>(
        [&](const auto& event) { requests.push_back(event.attribute_name); });
    const auto output = fixture.execute(kEdgeRatio);
    REQUIRE(fixture.mesh().face_attributes_.size() == 1);
    CHECK(fixture.mesh().solid_attributes_.empty());
    CHECK(requests == std::vector<std::string> { "f_mesh_quality_edge_ratio_1" });
    const auto infos = session::SessionQuery(fixture.model_layer).componentAttributeInfos(fixture.component_id);
    REQUIRE(infos.size() == 1);
    CHECK(infos.front().name == "f_mesh_quality_edge_ratio_1");
    CHECK(output.find("指标不支持 1") != std::string::npos);
}

TEST_CASE("MeshQuality retains partial Tet Collapse slots including zero and negative values", "[MeshQualityPlugin][QualityOutput]")
{
    auto mesh = makeHexMesh();
    mesh->solid_types_ = { VTK_TETRA, VTK_HEXAHEDRON, VTK_TETRA, VTK_TETRA };
    mesh->solid_vertices_ = { 0, 1, 3, 4, 0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 0, 3, 1, 4 };
    mesh->solid_vertices_offset_ = { 0, 4, 12, 16, 20 };
    QualityFixture fixture(std::move(mesh));
    const auto output = fixture.execute(kTetCollapse);
    REQUIRE(fixture.mesh().solid_attributes_.size() == 1);
    const auto& values = fixture.mesh().solid_attributes_.at("s_mesh_quality_tet_collapse_1");
    REQUIRE(values.size() == 4);
    CHECK(values[0] > 0.0);
    CHECK(std::isnan(values[1]));
    CHECK(values[2] == 0.0);
    CHECK(values[3] < 0.0);
    CHECK(output.find("有效 3") != std::string::npos);
    CHECK(output.find("不可用 1") != std::string::npos);
}
