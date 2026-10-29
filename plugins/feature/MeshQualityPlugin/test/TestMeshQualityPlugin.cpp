#include "ArgObject.h"
#include "ComponentData.h"
#include "EventBus.h"
#include "FeatureEvents.h"
#include "FeatureSystem.h"
#include "MeshData.h"
#include "MeshQualityHandler.h"
#include "ModelLayer.h"
#include "Selection.h"
#include "SolidCellRecognition.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <vtkCellType.h>
#include <vtkDoubleArray.h>
#include <vtkMeshQuality.h>
#include <vtkNew.h>

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
            { 0.5, height, 0 },
            { 1, 0, 0 },
            { 0, 0, 1 },
            { 0.5, height, 1 },
            { 1, 0, 1 },
        };
        ids = { 0, 2, 2, 1, 3, 5, 5, 4 };
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
        if (name.find("recognition_") != std::string::npos)
            continue;
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
    const auto output = fixture.execute(0);
    CHECK(output.find("有效 0") != std::string::npos);
    CHECK(output.find("不可用 1") != std::string::npos);
    CHECK(output.find("平均值") == std::string::npos);
    REQUIRE(fixture.mesh().solid_attributes_.at("s_mesh_quality_scaled_jacobian_1").size() == 1);
    CHECK(std::isnan(fixture.mesh().solid_attributes_.at("s_mesh_quality_scaled_jacobian_1")[0]));
}

TEST_CASE("MeshQuality preserves double precision under large coordinate translations", "[MeshQualityPlugin]")
{
    auto mesh = makeHexMesh();
    for (auto& point : mesh->vertex_positions_) {
        for (auto& coordinate : point)
            coordinate += 1e9;
    }
    QualityFixture fixture(std::move(mesh));
    fixture.execute(0);
    CHECK(fixture.mesh().solid_attributes_.at("s_mesh_quality_scaled_jacobian_1")[0] == Catch::Approx(1.0));
    fixture.execute(1);
    CHECK(fixture.mesh().solid_attributes_.at("s_mesh_quality_equiangle_skew_1")[0] == Catch::Approx(0.0).margin(1e-12));
    fixture.execute(2);
    CHECK(fixture.mesh().solid_attributes_.at("s_mesh_quality_edge_ratio_1")[0] == Catch::Approx(1.0));
}

TEST_CASE("MeshQuality excludes finite Verdict sentinels without repeated vertices", "[MeshQualityPlugin]")
{
    auto mesh = makeHexMesh();
    mesh->vertex_positions_[1] = { 1e-10, 0, 0 };
    QualityFixture fixture(std::move(mesh));
    const auto output = fixture.execute(0);
    CHECK(output.find("不可用 1") != std::string::npos);
    CHECK(std::isnan(fixture.mesh().solid_attributes_.at("s_mesh_quality_scaled_jacobian_1")[0]));
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
    fixture.execute(6);
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
    fixture.execute(0);
    fixture.execute(0);
    CHECK(fixture.mesh().face_attributes_.at("f_mesh_quality_scaled_jacobian_1")[0] == 42.0);
    CHECK(fixture.mesh().solid_attributes_.at("s_mesh_quality_scaled_jacobian_1")[0] == 43.0);
    CHECK(fixture.mesh().face_attributes_.size() == 2);
    CHECK(fixture.mesh().solid_attributes_.size() == 6);
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
    const int metric = GENERATE(0, 1, 2, 3, 4, 5, 6);
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
        // VTK 的底面 0/1/2 法向朝外，背离 3/4/5 面。
        mesh->vertex_positions_ = {
            { 0, 0, 0 },
            { 0.5, height, 0 },
            { 1, 0, 0 },
            { 0, 0, 1 },
            { 0.5, height, 1 },
            { 1, 0, 1 },
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
    const bool supported = is_face ? metric != 6 && (metric != 5 || cell_type == VTK_QUAD)
        : metric <= 2              ? metric != 2 || cell_type != VTK_PYRAMID
                                   : metric == 6 && cell_type == VTK_TETRA;
    const auto& attributes = is_face ? fixture.mesh().face_attributes_ : fixture.mesh().solid_attributes_;
    if (!supported && (is_face || metric == 3 || metric == 4 || metric == 5)) {
        CHECK(output.find("没有可计算的单元") != std::string::npos);
        CHECK(attributes.empty());
        return;
    }
    REQUIRE(attributes.size() == (is_face ? 1 : 5));
    const std::vector<std::string> keys { "scaled_jacobian", "equiangle_skew", "edge_ratio", "min_angle", "max_angle", "warpage", "tet_collapse" };
    const auto& values = attributes.at(std::string(is_face ? "f_" : "s_") + "mesh_quality_" + keys[metric] + "_1");
    REQUIRE(values.size() == 1);
    if (!supported) {
        CHECK(std::isnan(values[0]));
        CHECK(output.find("指标不支持 1") != std::string::npos);
        return;
    }
    double expected = 1.0;
    if (metric == 1)
        expected = 0.0;
    if (metric == 3 || metric == 4)
        expected = cell_type == VTK_TRIANGLE ? 60.0 : 90.0;
    if (metric == 6)
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
        fixture.execute(0);
        CHECK(fixture.mesh().solid_attributes_.at("s_mesh_quality_scaled_jacobian_1")[0] < 0.0);
        fixture.execute(6);
        CHECK(fixture.mesh().solid_attributes_.at("s_mesh_quality_tet_collapse_1")[0] < 0.0);
    }
    SECTION("flat tetrahedron with distinct vertices")
    {
        mesh->vertex_positions_[3] = { 1, 1, 0 };
        QualityFixture fixture(std::move(mesh));
        fixture.execute(0);
        CHECK(fixture.mesh().solid_attributes_.at("s_mesh_quality_scaled_jacobian_1")[0] == 0.0);
        fixture.execute(6);
        CHECK(fixture.mesh().solid_attributes_.at("s_mesh_quality_tet_collapse_1")[0] == 0.0);
    }
}

TEST_CASE("MeshQuality rejects malformed connectivity before calling VTK", "[MeshQualityPlugin]")
{
    auto mesh = makeHexMesh();
    SECTION("missing solid offsets") { mesh->solid_vertices_offset_.clear(); }
    SECTION("nonzero first offset")
    {
        mesh->solid_vertices_.insert(mesh->solid_vertices_.begin(), 7);
        mesh->solid_vertices_offset_ = { 1, 9 };
    }
    SECTION("unreferenced trailing indices") { mesh->solid_vertices_.push_back(0); }
    SECTION("invalid local point") { mesh->solid_vertices_[0] = 8; }
    QualityFixture fixture(std::move(mesh));
    const auto output = fixture.execute(0);
    CHECK(output.find("体：") != std::string::npos);
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

TEST_CASE("MeshQuality evaluates folded cells as their actual standard types", "[MeshQualityPlugin]")
{
    const int type = GENERATE(VTK_TETRA, VTK_PYRAMID, VTK_WEDGE);
    const int metric = GENERATE(0, 1, 2, 6);
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
    const std::vector<std::string> keys { "scaled_jacobian", "equiangle_skew", "edge_ratio", "min_angle", "max_angle", "warpage", "tet_collapse" };
    const std::string key = "s_mesh_quality_" + keys[metric] + "_1";
    const double expected = native.mesh().solid_attributes_.at(key)[0];
    const double actual = folded.mesh().solid_attributes_.at(key)[0];
    if (std::isfinite(expected))
        CHECK(actual == Catch::Approx(expected).margin(1e-12));
    else
        CHECK(std::isnan(actual));
    CHECK(output.find("折叠转换 1") != std::string::npos);
    CHECK(folded.mesh().solid_attributes_.at("s_mesh_quality_recognition_source_id_1") == std::vector<double> { 0 });
    CHECK(folded.mesh().solid_attributes_.at("s_mesh_quality_recognition_source_type_1") == std::vector<double> { VTK_HEXAHEDRON });
    CHECK(folded.mesh().solid_attributes_.at("s_mesh_quality_recognition_evaluated_type_1") == std::vector<double> { static_cast<double>(type) });
    CHECK(folded.mesh().solid_attributes_.at("s_mesh_quality_recognition_status_1") == std::vector<double> { 1 });
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
    QualityFixture fixture(std::move(mesh));
    const auto output = fixture.execute(2);
    const auto& values = fixture.mesh().solid_attributes_.at("s_mesh_quality_edge_ratio_1");
    REQUIRE(values.size() == 3);
    CHECK(std::isnan(values[0])); // 金字塔已识别，但 VTK 不支持该类型的 Edge Ratio。
    CHECK(std::isnan(values[1])); // 四个不同角点的自粘面不是标准四面体。
    CHECK(std::isfinite(values[2]));
    CHECK(output.find("类型未识别 1") != std::string::npos);
    CHECK(output.find("指标不支持 1") != std::string::npos);
    CHECK(fixture.mesh().solid_attributes_.at("s_mesh_quality_recognition_source_id_1") == std::vector<double> { 0, 1, 2 });
    CHECK(fixture.mesh().solid_attributes_.at("s_mesh_quality_recognition_evaluated_type_1") == std::vector<double> { VTK_PYRAMID, 0, VTK_TETRA });
    CHECK(fixture.mesh().solid_attributes_.at("s_mesh_quality_recognition_status_1") == std::vector<double> { 1, 2, 1 });
}

TEST_CASE("MeshQuality keeps inverted and flat folded tetrahedra signed", "[MeshQualityPlugin]")
{
    auto mesh = makeCollapsedMesh(VTK_TETRA, true);
    SECTION("inverted geometry")
    {
        for (auto& point : mesh->vertex_positions_)
            point[0] = -point[0];
        QualityFixture fixture(std::move(mesh));
        fixture.execute(0);
        CHECK(fixture.mesh().solid_attributes_.at("s_mesh_quality_scaled_jacobian_1")[0] < 0);
    }
    SECTION("zero height with distinct vertices")
    {
        mesh->vertex_positions_[3][2] = 0;
        QualityFixture fixture(std::move(mesh));
        fixture.execute(0);
        CHECK(fixture.mesh().solid_attributes_.at("s_mesh_quality_scaled_jacobian_1")[0] == 0);
    }
}

TEST_CASE("MeshQuality recognition records reuse names and preserve imported records on cleanup", "[MeshQualityPlugin]")
{
    auto mesh = makeCollapsedMesh(VTK_WEDGE, true);
    mesh->solid_attributes_["s_mesh_quality_recognition_status_1"] = { 99 };
    QualityFixture fixture(std::move(mesh));
    REQUIRE(fixture.feature_system.setFeatureActive("MeshQuality"));
    fixture.execute(0);
    fixture.execute(1);
    CHECK(fixture.mesh().solid_attributes_.size() == 7);
    CHECK(fixture.mesh().solid_attributes_.at("s_mesh_quality_recognition_status_1")[0] == 99);
    CHECK(fixture.mesh().solid_attributes_.at("s_mesh_quality_recognition_status_2_1")[0] == 1);
    REQUIRE(fixture.feature_system.setFeatureActive(""));
    REQUIRE(fixture.mesh().solid_attributes_.size() == 1);
    CHECK(fixture.mesh().solid_attributes_.at("s_mesh_quality_recognition_status_1")[0] == 99);
}
