/**
 * @file TestMModelHandler.cpp
 * @brief M (.m) 网格文件处理器单元测试
 *
 * MModelHandler 直接解析 / 生成 .m 文本读写网格，当前写出入口为组件化
 * write_components()。MeshData 自包含（vertex_positions_ 常驻坐标、连通性存组件内局部点索引），
 * 测试将源 MeshData 加入 ModelLayer 后即可直接导出，无需全局点池换算。
 * .m 格式以表面三角网格为主，不支持体单元，写出后体信息不会回流。
 * 点/面/边 {...} 属性段经 v_ / f_ / e_ 前缀和分量数后缀命名的属性表回环。
 */
#include "MModelHandler.h"
#include "ComponentData.h"
#include "MeshData.h"
#include "ModelLayer.h"
#include "ModelPayload.h"
#include "TempFile.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace fs = std::filesystem;

namespace {
/** @brief 通过原读取实现验证输入流故障，不改变处理器的对外文件入口 */
class TestableMModelHandler : public systems::io::MModelHandler {
public:
    using MModelHandler::readMesh;
};

/** @brief 两遍读取过程中可注入故障的阶段 */
enum class ReadFailurePhase { FirstPass,
    Rewind,
    SecondPass };

/** @brief 在读到末尾或回退时注入故障，避免依赖操作系统磁盘错误 */
class FaultingInputBuffer : public std::stringbuf {
public:
    /** @brief 用合法网格文本构造缓冲区，并指定失败阶段 */
    explicit FaultingInputBuffer(ReadFailurePhase phase)
        : std::stringbuf("Vertex 4 0 0 0\nVertex 6911 1 0 0\nEdge 4 6911 {sharp}\n", std::ios::in)
        , phase_(phase)
    {
    }

protected:
    /** @brief 读到缓冲区末尾时模拟对应扫描阶段的 I/O 故障 */
    int_type underflow() override
    {
        if (phase_ == ReadFailurePhase::FirstPass || (phase_ == ReadFailurePhase::SecondPass && rewound_))
            throw std::ios_base::failure("injected read failure");
        return std::stringbuf::underflow();
    }

    /** @brief 模拟回退失败，或记录第二次扫描已开始 */
    pos_type seekoff(off_type offset, std::ios_base::seekdir direction, std::ios_base::openmode mode) override
    {
        if (phase_ == ReadFailurePhase::Rewind)
            return pos_type(off_type(-1));
        rewound_ = true;
        return std::stringbuf::seekoff(offset, direction, mode);
    }

private:
    ReadFailurePhase phase_;
    bool rewound_ { false };
};

/** @brief 在真实文件入口内注入读取故障，验证异常不会离开插件入口 */
class FaultingMModelHandler : public systems::io::MModelHandler {
public:
    /** @brief 指定需要验证的读取失败阶段 */
    explicit FaultingMModelHandler(ReadFailurePhase phase)
        : phase_(phase)
    {
    }

protected:
    /** @brief 将真实解析器连接到故障流，仍由 read_model 负责捕获异常 */
    std::unique_ptr<MeshData> readMesh(std::istream&, const fs::path& path) override
    {
        FaultingInputBuffer buffer(phase_);
        std::istream input(&buffer);
        return MModelHandler::readMesh(input, path);
    }

private:
    ReadFailurePhase phase_;
};

/** @brief 模拟非标准异常，验证插件入口的未知异常兜底 */
class UnknownExceptionMModelHandler : public systems::io::MModelHandler {
protected:
    /** @brief 使用非 std::exception 异常模拟读取过程中未知的失败 */
    std::unique_ptr<MeshData> readMesh(std::istream&, const fs::path&) override
    {
        throw 1;
    }
};

/**
 * @brief 构造一个仅含表面三角形的最小 MeshData
 *
 * 源数据仍带一个 patch/block，用于验证写出不依赖（也不再保留）分组信息。
 */
MeshData MakeSurfaceTriMesh()
{
    MeshData m;
    m.init();
    m.vertex_positions_ = {
        { 0.0, 0.0, 0.0 },
        { 1.0, 0.0, 0.0 },
        { 0.0, 1.0, 0.0 },
        { 0.0, 0.0, 1.0 },
    };
    m.face_vertices_ = {
        0, 2, 1,
        0, 1, 3,
        0, 3, 2,
        1, 2, 3,
    };
    m.face_vertices_offset_ = { 0, 3, 6, 9, 12 };

    auto patch = std::make_unique<Patch>(1, 1);
    patch->faces = { 0, 1, 2, 3 };
    m.patches_[1] = std::move(patch);

    auto block = std::make_unique<Block>();
    block->id = 1;
    block->patchIDs = { 1 };
    m.blocks_[1] = std::move(block);
    return m;
}

std::vector<Index> addMeshModelAndGetComponentIds(
    ModelLayer& layer,
    std::unique_ptr<MeshData> mesh)
{
    ComponentDatas comps;
    auto comp = std::make_unique<ComponentData>();
    comp->id = -1;
    comp->mesh = std::move(mesh);
    comps.push_back(std::move(comp));
    const Index modelId = layer.addModel("model", std::move(comps));
    REQUIRE(modelId >= 0);

    std::vector<Index> componentIds = layer.modelById(modelId)->componentIds();
    REQUIRE(!componentIds.empty());

    return componentIds;
}

const MeshData* requireReadableMeshModel(const ModelPayload& payload)
{
    const auto& components = payload.components;
    REQUIRE(!components.empty());

    for (const auto& component : components) {
        if (component && component->mesh) {
            return component->mesh.get();
        }
    }

    FAIL("read model does not contain MeshData component");
    return nullptr;
}
} // namespace

TEST_CASE("MModelHandler::write_components()/read_model() round-trip")
{
    systems::io::MModelHandler io;

    fs::path out;
    SECTION("Latin path")
    {
        out = core::TempFile::instance().path().string() + ".m";
    }
    SECTION("Chinese filename")
    {
        out = core::TempFile::instance().path();
        out.replace_filename("中文_" + out.stem().string() + ".m");
    }

    // MeshData 含 unique_ptr，不可拷贝；分别构造源与参照
    ModelLayer layer;
    std::vector<Index> componentIds = addMeshModelAndGetComponentIds(
        layer,
        std::make_unique<MeshData>(MakeSurfaceTriMesh()));

    REQUIRE_NOTHROW(io.write_components(layer, componentIds, out, {}));
    REQUIRE(fs::exists(out));

    std::optional<ModelPayload> payload;
    REQUIRE_NOTHROW(payload = io.read_model(out, {}));
    REQUIRE(payload.has_value());

    const MeshData* read_mesh = requireReadableMeshModel(*payload);

    const MeshData ref = MakeSurfaceTriMesh();
    // 顶点数应一致
    REQUIRE(read_mesh->vertex_positions_.size() == ref.vertex_positions_.size());
    // 面数应一致
    REQUIRE(read_mesh->face_vertices_offset_.size() == ref.face_vertices_offset_.size());
    REQUIRE(read_mesh->face_vertices_.size() == ref.face_vertices_.size());

    // 顶点坐标精确对比（.m 是文本格式，应可精确恢复）
    for (size_t i = 0; i < ref.vertex_positions_.size(); ++i) {
        for (int k = 0; k < 3; ++k) {
            REQUIRE(read_mesh->vertex_positions_[i][k] == ref.vertex_positions_[i][k]);
        }
    }

    // patches_/blocks_ 已废弃，.m 读写不再维护 patch 分组，
    // 读回网格不重建 patches_，面数据完整回流即可。
    REQUIRE(read_mesh->patches_.empty());
}

TEST_CASE("MModelHandler::attribute round-trip")
{
    systems::io::MModelHandler io;
    fs::path out = core::TempFile::instance().path().string() + "_attr.m";

    // 点/面属性按 v_<key>_<分量数> / f_<key>_<分量数> 命名，
    // 经 .m {...} 属性段写出并读回后应原样保留
    MeshData m = MakeSurfaceTriMesh();
    m.vertex_attributes_["v_rgb_3"] = {
        1.0, 0.0, 0.0,
        0.0, 1.0, 0.0,
        0.0, 0.0, 1.0,
        0.5, 0.5, 0.5,
    };
    m.face_attributes_["f_g_1"] = { 1.0, 1.0, 2.0, 2.0 };
    const std::vector<double> ref_rgb = m.vertex_attributes_["v_rgb_3"];
    const std::vector<double> ref_g = m.face_attributes_["f_g_1"];

    ModelLayer layer;
    std::vector<Index> componentIds = addMeshModelAndGetComponentIds(
        layer,
        std::make_unique<MeshData>(std::move(m)));

    REQUIRE_NOTHROW(io.write_components(layer, componentIds, out, {}));
    REQUIRE(fs::exists(out));

    std::optional<ModelPayload> payload;
    REQUIRE_NOTHROW(payload = io.read_model(out, {}));
    REQUIRE(payload.has_value());

    const MeshData* read_mesh = requireReadableMeshModel(*payload);
    REQUIRE(read_mesh->vertex_attributes_.count("v_rgb_3") == 1);
    REQUIRE(read_mesh->vertex_attributes_.at("v_rgb_3") == ref_rgb);
    REQUIRE(read_mesh->face_attributes_.count("f_g_1") == 1);
    REQUIRE(read_mesh->face_attributes_.at("f_g_1") == ref_g);
}

TEST_CASE("MModelHandler::write_components() without patches preserves faces")
{
    systems::io::MModelHandler io;
    fs::path out = core::TempFile::instance().path().string() + "_empty.m";

    MeshData m;
    m.init();
    m.vertex_positions_ = { { 0.0, 0.0, 0.0 }, { 1.0, 0.0, 0.0 }, { 0.0, 1.0, 0.0 } };
    m.face_vertices_ = { 0, 1, 2 };
    m.face_vertices_offset_ = { 0, 3 };
    // 注意：没有 patches_（patches_/blocks_ 已废弃，写出不再依赖分组信息）

    ModelLayer layer;
    std::vector<Index> componentIds = addMeshModelAndGetComponentIds(
        layer,
        std::make_unique<MeshData>(std::move(m)));

    REQUIRE_NOTHROW(io.write_components(layer, componentIds, out, {}));
    REQUIRE(fs::exists(out));

    // 无 patch 时面数据仍应完整写出并读回
    std::optional<ModelPayload> payload;
    REQUIRE_NOTHROW(payload = io.read_model(out, {}));
    REQUIRE(payload.has_value());

    const MeshData* read_mesh = requireReadableMeshModel(*payload);
    // 读回应保留写出的 1 个三角形面
    REQUIRE(read_mesh->face_vertices_offset_.size() == 2);
}

TEST_CASE("MModelHandler::read_model() - model_name preserved")
{
    systems::io::MModelHandler io;
    fs::path out = core::TempFile::instance().path().string() + "_name.m";

    ModelLayer layer;
    std::vector<Index> componentIds = addMeshModelAndGetComponentIds(
        layer,
        std::make_unique<MeshData>(MakeSurfaceTriMesh()));

    REQUIRE_NOTHROW(io.write_components(layer, componentIds, out, {}));
    REQUIRE(fs::exists(out));

    std::optional<ModelPayload> payload;
    REQUIRE_NOTHROW(payload = io.read_model(out, {}));
    REQUIRE(payload.has_value());
    REQUIRE(payload->model_name == out.filename().u8string());
}

TEST_CASE("MModelHandler::read_model() preserves Edge attributes", "[MModelHandler][Edge]")
{
    systems::io::MModelHandler io;
    const fs::path path = core::TempFile::instance().path().string() + "_edges.m";
    const std::string vertices = "Vertex 1946 0 0 0\n"
                                 "Vertex 748 1 0 0\n"
                                 "Vertex 42 0 1 0\n";
    const std::string edges = "Edge 42 1946\n"
                              "Edge 1946 748 {loop_id=(9) candidate_id=(0) segment_id=(0) loop_type=(3) source_path_id=(0) is_sharp=(0) accepted=(1) reject_reason=(0) homology_ok=(1) energy=(1.9067301061999999) length_term=(1.876211718) planarity_term=(0.030518388216) concave_score=(0)}\n"
                              "Edge 748 42 {loop_id=(2) direction=(1 0 -1)}\n"
                              "Edge 748 1946\n";

    // Edge 只有两个端点编号；两遍读取应兼容边记录出现在顶点之前、之后或中间。
    std::ofstream output(path);
    REQUIRE(output.is_open());
    SECTION("Edges after vertices")
    {
        output << vertices << "Face 7 1946 748 42 {g=(5)}\n"
               << edges;
    }
    SECTION("Edges before vertices")
    {
        output << edges << vertices << "Face 7 1946 748 42 {g=(5)}\n";
    }
    SECTION("Edges between vertices")
    {
        const size_t first_vertex_end = vertices.find('\n') + 1;
        output << vertices.substr(0, first_vertex_end) << edges << vertices.substr(first_vertex_end)
               << "Face 7 1946 748 42 {g=(5)}\n";
    }
    output.close();

    const auto payload = io.read_model(path, { });
    REQUIRE(payload.has_value());
    const MeshData* mesh = requireReadableMeshModel(*payload);
    const std::vector<Index> expected_edges = { 2, 0, 0, 1, 1, 2, 1, 0 };
    REQUIRE(mesh->edge_vertices_ == expected_edges);
    REQUIRE(mesh->vertex_count_ == 3);
    const std::vector<Index> expected_faces = { 0, 1, 2 };
    REQUIRE(mesh->face_vertices_ == expected_faces);
    REQUIRE(mesh->face_attributes_.at("f_g_1") == std::vector<double> { 5.0 });

    // 按边单元顺序验证完整示例的全部标量，缺失属性的前后边均须补零。
    const std::map<std::string, double> expected_scalars = {
        { "loop_id", 9.0 }, { "candidate_id", 0.0 }, { "segment_id", 0.0 },
        { "loop_type", 3.0 }, { "source_path_id", 0.0 }, { "is_sharp", 0.0 },
        { "accepted", 1.0 }, { "reject_reason", 0.0 }, { "homology_ok", 1.0 },
        { "energy", 1.9067301061999999 }, { "length_term", 1.876211718 },
        { "planarity_term", 0.030518388216 }, { "concave_score", 0.0 }
    };
    REQUIRE(mesh->edge_attributes_.size() == expected_scalars.size() + 1);
    for (const auto& [key, value] : expected_scalars) {
        const std::string name = "e_" + key + "_1";
        CAPTURE(name);
        const std::vector<double> expected = { 0.0, value, key == "loop_id" ? 2.0 : 0.0, 0.0 };
        REQUIRE(mesh->edge_attributes_.at(name) == expected);
    }
    const std::vector<double> expected_direction = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, -1.0, 0.0, 0.0, 0.0 };
    REQUIRE(mesh->edge_attributes_.at("e_direction_3") == expected_direction);
    REQUIRE(mesh->vertex_attributes_.empty());

    // 再导出并读回，全部边属性（含高精度 double 和多分量值）应精确保留。
    ModelLayer layer;
    const auto component_ids = addMeshModelAndGetComponentIds(layer, mesh->clone());
    const fs::path exported_path = core::TempFile::instance().path().string() + "_edges_exported.m";
    io.write_components(layer, component_ids, exported_path, { });
    const auto exported_payload = io.read_model(exported_path, { });
    REQUIRE(exported_payload.has_value());
    const MeshData* exported_mesh = requireReadableMeshModel(*exported_payload);
    REQUIRE(exported_mesh->edge_vertices_ == mesh->edge_vertices_);
    REQUIRE(exported_mesh->edge_attributes_ == mesh->edge_attributes_);
    REQUIRE(exported_mesh->face_attributes_ == mesh->face_attributes_);
    REQUIRE(exported_mesh->vertex_positions_ == mesh->vertex_positions_);
}

TEST_CASE("MModelHandler::write_components() preserves edges without attributes", "[MModelHandler][Edge]")
{
    systems::io::MModelHandler io;
    const fs::path path = core::TempFile::instance().path().string() + "_bare_edges.m";
    auto mesh = std::make_unique<MeshData>();
    mesh->init();
    mesh->vertex_positions_ = { { 0.0, 0.0, 0.0 }, { 1.9067301061999999, 0.0, 0.0 } };
    mesh->edge_vertices_ = { 1, 0 };
    const auto expected_positions = mesh->vertex_positions_;
    ModelLayer layer;
    const auto component_ids = addMeshModelAndGetComponentIds(layer, std::move(mesh));
    io.write_components(layer, component_ids, path, { });

    // 独立检查文本语法：端点从 1 起编号，无属性时不输出空属性段。
    std::ifstream input(path);
    REQUIRE(input.is_open());
    std::string line;
    size_t edge_lines = 0;
    while (std::getline(input, line)) {
        if (line.rfind("Edge ", 0) == 0) {
            REQUIRE(line == "Edge 2 1");
            ++edge_lines;
        }
    }
    REQUIRE(edge_lines == 1);

    const auto payload = io.read_model(path, { });
    REQUIRE(payload.has_value());
    const MeshData* read_mesh = requireReadableMeshModel(*payload);
    const std::vector<Index> expected_edges = { 1, 0 };
    REQUIRE(read_mesh->edge_vertices_ == expected_edges);
    REQUIRE(read_mesh->edge_attributes_.empty());
    REQUIRE(read_mesh->face_vertices_.empty());
    REQUIRE(read_mesh->vertex_positions_ == expected_positions);
}

TEST_CASE("MModelHandler::read_model() skips invalid Edge records", "[MModelHandler][Edge]")
{
    systems::io::MModelHandler io;
    const fs::path path = core::TempFile::instance().path().string() + "_invalid_edges.m";
    std::ofstream output(path);
    REQUIRE(output.is_open());
    // 非法端点、缺失端点和自环不能产生边单元或让后续合法边的属性错位。
    output << "Vertex 10 0 0 0\nVertex 20 1 0 0\n"
              "Edge 10 99 {invalid=(1)}\n"
              "Edge 10 {invalid=(2)}\n"
              "Edge 10 10 {invalid=(3)}\n"
              "Edge 10 20 {accepted=(1)}\n";
    output.close();

    const auto payload = io.read_model(path, { });
    REQUIRE(payload.has_value());
    const MeshData* mesh = requireReadableMeshModel(*payload);
    const std::vector<Index> expected_edges = { 0, 1 };
    REQUIRE(mesh->edge_vertices_ == expected_edges);
    REQUIRE(mesh->edge_attributes_.size() == 1);
    REQUIRE(mesh->edge_attributes_.at("e_accepted_1") == std::vector<double> { 1.0 });
}

TEST_CASE("MModelHandler::boolean flags round-trip", "[MModelHandler][Attributes]")
{
    systems::io::MModelHandler io;
    const fs::path path = core::TempFile::instance().path().string() + "_flags.m";
    std::ofstream output(path);
    REQUIRE(output.is_open());
    // 覆盖独立标记、连续标记、与数值属性混写，以及显式零值和缺失值。
    output << "Vertex 4 0 0 0 {corner}\n"
              "Vertex 6911 1 0 0\n"
              "Vertex 42 0 1 0\n"
              "Face 1 4 6911 42 {selected group=(2)}\n"
              "Edge 42 4 {sharp=(0)}\n"
              "Edge 4 6911 {sharp}\n"
              "Edge 6911 42 {sharp boundary energy = (1.9067301061999999) pinned rgb=(1 0 -1) is_sharp=(0) tail}\n"
              "Edge 4 42\n";
    output.close();

    const auto payload = io.read_model(path, { });
    REQUIRE(payload.has_value());
    const MeshData* mesh = requireReadableMeshModel(*payload);
    const std::map<std::string, std::vector<double>> expected_vertex_attributes = {
        { "v_corner_1", { 1.0, 0.0, 0.0 } }
    };
    const std::map<std::string, std::vector<double>> expected_face_attributes = {
        { "f_selected_1", { 1.0 } }, { "f_group_1", { 2.0 } }
    };
    const std::map<std::string, std::vector<double>> expected_edge_attributes = {
        { "e_sharp_1", { 0.0, 1.0, 1.0, 0.0 } },
        { "e_boundary_1", { 0.0, 0.0, 1.0, 0.0 } },
        { "e_energy_1", { 0.0, 0.0, 1.9067301061999999, 0.0 } },
        { "e_pinned_1", { 0.0, 0.0, 1.0, 0.0 } },
        { "e_rgb_3", { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, -1.0, 0.0, 0.0, 0.0 } },
        { "e_is_sharp_1", { 0.0, 0.0, 0.0, 0.0 } },
        { "e_tail_1", { 0.0, 0.0, 1.0, 0.0 } }
    };
    REQUIRE(mesh->vertex_attributes_ == expected_vertex_attributes);
    REQUIRE(mesh->face_attributes_ == expected_face_attributes);
    REQUIRE(mesh->edge_attributes_ == expected_edge_attributes);

    // 统一导出为数值形式，标记的真值和零值均应能完整回读。
    ModelLayer layer;
    const auto component_ids = addMeshModelAndGetComponentIds(layer, mesh->clone());
    const fs::path exported_path = core::TempFile::instance().path().string() + "_flags_exported.m";
    io.write_components(layer, component_ids, exported_path, { });
    std::ifstream input(exported_path);
    REQUIRE(input.is_open());
    std::string line;
    bool has_sharp_true = false;
    bool has_sharp_false = false;
    // 独立验证 sharp 的数值属性语法，允许括号、等号前后任意空白。
    const std::regex sharp_trait(R"((?:\{|\s)sharp\s*=\s*\(\s*([01])\s*\))");
    while (std::getline(input, line)) {
        std::smatch match;
        if (std::regex_search(line, match, sharp_trait)) {
            has_sharp_true = has_sharp_true || match[1] == "1";
            has_sharp_false = has_sharp_false || match[1] == "0";
        }
    }
    REQUIRE(has_sharp_true);
    REQUIRE(has_sharp_false);

    const auto exported_payload = io.read_model(exported_path, { });
    REQUIRE(exported_payload.has_value());
    const MeshData* exported_mesh = requireReadableMeshModel(*exported_payload);
    REQUIRE(exported_mesh->vertex_attributes_ == expected_vertex_attributes);
    REQUIRE(exported_mesh->face_attributes_ == expected_face_attributes);
    REQUIRE(exported_mesh->edge_attributes_ == expected_edge_attributes);
    REQUIRE(exported_mesh->edge_vertices_ == mesh->edge_vertices_);
}

TEST_CASE("MModelHandler::read_model() filters malformed flag names", "[MModelHandler][Attributes]")
{
    systems::io::MModelHandler io;
    const fs::path path = core::TempFile::instance().path().string() + "_invalid_flags.m";
    std::ofstream output(path);
    REQUIRE(output.is_open());
    // 首字母合法仍可能是畸形名称；过滤后须保留后面的合法标记和数值属性。
    output << "Vertex 4 0 0 0 {corner b(1) 123 9flag .noise bad-name ; () _protected flag2 value.name=(2)}\n"
              "Vertex 6911 1 0 0\n"
              "Vertex 42 0 1 0\n"
              "Face 1 4 6911 42 {selected bad!name group=(3)}\n"
              "Edge 4 6911 {a b(1) sharp loop_id=(9)}\n";
    output.close();

    const auto payload = io.read_model(path, { });
    REQUIRE(payload.has_value());
    const MeshData* mesh = requireReadableMeshModel(*payload);
    const std::map<std::string, std::vector<double>> expected_vertex_attributes = {
        { "v_corner_1", { 1.0, 0.0, 0.0 } }, { "v__protected_1", { 1.0, 0.0, 0.0 } },
        { "v_flag2_1", { 1.0, 0.0, 0.0 } }, { "v_value.name_1", { 2.0, 0.0, 0.0 } }
    };
    const std::map<std::string, std::vector<double>> expected_face_attributes = {
        { "f_selected_1", { 1.0 } }, { "f_group_1", { 3.0 } }
    };
    const std::map<std::string, std::vector<double>> expected_edge_attributes = {
        { "e_a_1", { 1.0 } }, { "e_sharp_1", { 1.0 } }, { "e_loop_id_1", { 9.0 } }
    };
    REQUIRE(mesh->vertex_attributes_ == expected_vertex_attributes);
    REQUIRE(mesh->face_attributes_ == expected_face_attributes);
    REQUIRE(mesh->edge_attributes_ == expected_edge_attributes);
}

TEST_CASE("MModelHandler::read_model() preserves many forward-referenced Edge records", "[MModelHandler][Edge]")
{
    systems::io::MModelHandler io;
    const fs::path path = core::TempFile::instance().path().string() + "_many_edges.m";
    std::ofstream output(path);
    REQUIRE(output.is_open());
    constexpr size_t edge_count = 4096;
    // 批量边先于顶点且方向交替，序号属性必须与原始记录逐条对齐。
    for (size_t i = 0; i < edge_count; ++i)
        output << (i % 2 == 0 ? "Edge 4 6911" : "Edge 6911 4") << " {sequence=(" << i << ")}\n";
    // 最后一行不带换行，验证扫描到 EOF 后第二遍仍能正确重新读取。
    output << "Vertex 4 0 0 0\nVertex 6911 1 0 0";
    output.close();

    const auto payload = io.read_model(path, { });
    REQUIRE(payload.has_value());
    const MeshData* mesh = requireReadableMeshModel(*payload);
    REQUIRE(mesh->edge_vertices_.size() == edge_count * 2);
    const auto& sequences = mesh->edge_attributes_.at("e_sequence_1");
    REQUIRE(sequences.size() == edge_count);
    for (size_t i = 0; i < edge_count; ++i) {
        CAPTURE(i);
        REQUIRE(mesh->edge_vertices_[i * 2] == static_cast<Index>(i % 2));
        REQUIRE(mesh->edge_vertices_[i * 2 + 1] == static_cast<Index>(1 - i % 2));
        REQUIRE(sequences[i] == static_cast<double>(i));
    }
}

TEST_CASE("MModelHandler::read_model() returns nullopt when opening fails", "[MModelHandler][Errors]")
{
    systems::io::MModelHandler io;
    const fs::path path = core::TempFile::instance().path().string() + "_missing.m";
    REQUIRE_FALSE(fs::exists(path));
    std::optional<ModelPayload> payload;
    REQUIRE_NOTHROW(payload = io.read_model(path, { }));
    REQUIRE_FALSE(payload.has_value());
}

TEST_CASE("MModelHandler::readMesh() reports stream failures", "[MModelHandler][Errors]")
{
    ReadFailurePhase phase = ReadFailurePhase::FirstPass;
    std::string expected_error = "failed to read input file";
    SECTION("First pass fails after partial input")
    {
        phase = ReadFailurePhase::FirstPass;
    }
    SECTION("Rewind fails")
    {
        phase = ReadFailurePhase::Rewind;
        expected_error = "failed to rewind input file";
    }
    SECTION("Second pass fails after partial input")
    {
        phase = ReadFailurePhase::SecondPass;
    }
    FaultingInputBuffer buffer(phase);
    std::istream input(&buffer);
    TestableMModelHandler io;
    // 失败必须抛异常而非返回已解析的部分网格；使用真实读取流程验证。
    REQUIRE_THROWS_WITH(io.readMesh(input, "fault.m"),
        Catch::Matchers::ContainsSubstring(expected_error));
}

TEST_CASE("MModelHandler::read_model() contains stream failures at plugin boundary", "[MModelHandler][Errors]")
{
    ReadFailurePhase phase = ReadFailurePhase::FirstPass;
    SECTION("First pass fails after partial input")
    {
        phase = ReadFailurePhase::FirstPass;
    }
    SECTION("Rewind fails")
    {
        phase = ReadFailurePhase::Rewind;
    }
    SECTION("Second pass fails after partial input")
    {
        phase = ReadFailurePhase::SecondPass;
    }

    const fs::path path = core::TempFile::instance().path().string() + "_boundary.m";
    std::ofstream output(path);
    REQUIRE(output.is_open());
    output << "Vertex 4 0 0 0\nVertex 6911 1 0 0\nEdge 4 6911 {sharp}\n";
    output.close();

    FaultingMModelHandler io(phase);
    systems::io::ModelIOHandler& entry = io;
    std::optional<ModelPayload> payload;
    // 经宿主使用的虚接口进入文件入口，读取失败不能抛出或返回部分数据。
    REQUIRE_NOTHROW(payload = entry.read_model(path, { }));
    REQUIRE_FALSE(payload.has_value());
}

TEST_CASE("MModelHandler::read_model() contains unknown exceptions at plugin boundary", "[MModelHandler][Errors]")
{
    const fs::path path = core::TempFile::instance().path().string() + "_unknown_exception.m";
    std::ofstream output(path);
    REQUIRE(output.is_open());
    output << "Vertex 4 0 0 0\n";
    output.close();

    UnknownExceptionMModelHandler io;
    systems::io::ModelIOHandler& entry = io;
    std::optional<ModelPayload> payload;
    REQUIRE_NOTHROW(payload = entry.read_model(path, { }));
    REQUIRE_FALSE(payload.has_value());
}

TEST_CASE("MModelHandler::read_model() matches complete Edge keywords", "[MModelHandler][Edge]")
{
    systems::io::MModelHandler io;
    const fs::path path = core::TempFile::instance().path().string() + "_keywords.m";
    std::ofstream output(path);
    REQUIRE(output.is_open());
    output << "Vertex 4 0 0 0\nVertex 6911 1 0 0\n"
              "EdgeExtra 4 6911 {invalid}\n"
              "Edge4 4 6911 {invalid}\n"
              "edge 4 6911 {invalid}\n"
              "# Edge 4 6911 {invalid}\n"
              "  \tEdge\t4 6911 {sequence=(7)}\n";
    output.close();
    const auto payload = io.read_model(path, { });
    REQUIRE(payload.has_value());
    const MeshData* mesh = requireReadableMeshModel(*payload);
    const std::vector<Index> expected_edges = { 0, 1 };
    REQUIRE(mesh->edge_vertices_ == expected_edges);
    const std::map<std::string, std::vector<double>> expected_attributes = { { "e_sequence_1", { 7.0 } } };
    REQUIRE(mesh->edge_attributes_ == expected_attributes);
}
