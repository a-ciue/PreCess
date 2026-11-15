#include "ObjMeshIO.h"

// MeshData 使用 double；解析阶段若先落到 float，后续算法导出无法恢复丢失的坐标精度。
// 各平台使用仓库自带的 tinyobj，独立命名空间避免与静态依赖中的同名符号冲突。
#define TINYOBJLOADER_USE_DOUBLE
#define TINYOBJLOADER_IMPLEMENTATION
#define tinyobj precess_tinyobj
#include "tiny_obj_loader.h"
#undef tinyobj

#include <fstream>
#include <spdlog/spdlog.h>
#include <streambuf>
#include <string_view>
#include <type_traits>
#include <unordered_map>

static_assert(std::is_same_v<precess_tinyobj::real_t, double>);

namespace {
//! @brief 为当前对象内的同名组提供相同的解析标识，保留原始名称和行号。
class ObjGroupInput : public std::streambuf {
public:
    explicit ObjGroupInput(std::streambuf& source)
        : source_(source)
    {
    }

    std::string_view originalName(const std::string& key) const
    {
        const auto it = names_.find(key);
        return it == names_.end() ? key : it->second;
    }

private:
    int_type underflow() override
    {
        auto next = source_.sbumpc();
        if (next == traits_type::eof())
            return traits_type::eof();

        // 与 tinyobj 一样，将 LF、CRLF 和单独的 CR 视为换行。
        line_.clear();
        while (next != traits_type::eof() && next != '\r' && next != '\n') {
            line_.push_back(traits_type::to_char_type(next));
            next = source_.sbumpc();
        }
        if (next == '\r' && source_.sgetc() == '\n')
            source_.sbumpc();

        std::string_view record(line_);
        if (first_line_ && record.starts_with("\xEF\xBB\xBF"))
            record.remove_prefix(3);
        first_line_ = false;
        rewriteSection(record.substr(0, record.find('\0')));
        line_.push_back('\n');
        setg(line_.data(), line_.data(), line_.data() + line_.size());
        return traits_type::to_int_type(*gptr());
    }

    void rewriteSection(std::string_view record)
    {
        constexpr std::string_view spaces = " \t";
        const auto start = record.find_first_not_of(spaces);
        if (start == std::string_view::npos)
            return;
        record.remove_prefix(start);
        if (record.size() < 2 || spaces.find(record[1]) == std::string_view::npos)
            return;

        // tinyobj 用同一个 shape.name 存放对象名和组名，两类名称都使用独立标识。
        if (record.front() == 'o') {
            group_keys_.clear();
            current_group_.clear();
            setSectionLine('o', registerName(record.substr(2)));
            return;
        }
        if (record.front() != 'g')
            return;

        record.remove_prefix(1);
        std::string group_name;
        while (true) {
            const auto name_start = record.find_first_not_of(spaces);
            if (name_start == std::string_view::npos || record[name_start] == '#')
                break;
            record.remove_prefix(name_start);
            const auto name_end = record.find_first_of(spaces);
            if (!group_name.empty())
                group_name.push_back(' ');
            group_name.append(record.substr(0, name_end));
            record.remove_prefix(name_end == std::string_view::npos ? record.size() : name_end);
        }

        if (group_name.empty()) {
            current_group_.clear();
            return; // 保留空组声明及 tinyobj 原有的诊断。
        }

        auto [it, inserted] = group_keys_.try_emplace(std::move(group_name));
        if (inserted)
            it->second = registerName(it->first);
        if (it->second == current_group_) {
            line_.clear(); // 保留空行，使解析告警仍对应原始文件的行号。
            return;
        }
        current_group_ = it->second;
        setSectionLine('g', current_group_);
    }

    std::string registerName(std::string_view name)
    {
        auto key = std::to_string(names_.size());
        names_.emplace(key, name);
        return key;
    }

    void setSectionLine(char kind, std::string_view key)
    {
        line_.assign(1, kind);
        line_.push_back(' ');
        line_.append(key);
    }

    std::streambuf& source_;
    std::string line_;
    std::unordered_map<std::string, std::string> names_; // 解析标识 -> 原始对象名或组名
    std::unordered_map<std::string, std::string> group_keys_; // 当前对象内组名 -> 解析标识
    std::string current_group_;
    bool first_line_ = true;
};
} // namespace

std::optional<ComponentDatas> ObjMeshIO::loadFromFile(const std::filesystem::path& filename)
{
    const auto file_name = filename.string();
    std::ifstream file(file_name);
    if (!file) {
        spdlog::error("TinyObjReader: Cannot open file [{}]", file_name);
        return std::nullopt;
    }

    ObjGroupInput group_input(*file.rdbuf());
    std::istream input(&group_input);
    precess_tinyobj::attrib_t attrib;
    std::vector<precess_tinyobj::shape_t> shapes;
    std::vector<precess_tinyobj::material_t> materials;
    std::string warning;
    std::string error;
    precess_tinyobj::MaterialFileReader material_reader(std::filesystem::path("./").make_preferred().string());
    if (!precess_tinyobj::LoadObj(&attrib, &shapes, &materials, &warning, &error,
            &input, &material_reader, /*triangulate=*/false)) {
        if (!error.empty())
            spdlog::error("TinyObjReader: {}", error);
        return std::nullopt;
    }

    if (!warning.empty())
        spdlog::warn("TinyObjReader: {}", warning);

    // 同一组可以跨多个 shape。按首次遇到面的顺序创建组件，保留每组内的面序。
    std::vector<std::vector<const precess_tinyobj::shape_t*>> component_shapes;
    std::unordered_map<std::string_view, size_t> component_by_key;
    for (const auto& shape : shapes) {
        if (shape.mesh.num_face_vertices.empty())
            continue;
        if (!shape.name.empty()) {
            auto [it, inserted] = component_by_key.try_emplace(shape.name, component_shapes.size());
            if (!inserted) {
                component_shapes[it->second].push_back(&shape);
                continue;
            }
        }
        component_shapes.push_back({ &shape });
    }

    ComponentDatas result;
    result.reserve(component_shapes.size());
    for (const auto& fragments : component_shapes) {
        auto mesh_data = std::make_unique<MeshData>();
        mesh_data->face_vertices_offset_.push_back(0);

        std::unordered_map<int, Index> global_to_local; // OBJ 全局点索引 -> 组件内局部点索引
        for (const auto* fragment : fragments) {
            const auto& mesh = fragment->mesh;
            size_t corner = 0;
            for (unsigned char face_vertex_count : mesh.num_face_vertices) {
                for (size_t k = 0; k < face_vertex_count; k++, corner++) {
                    const int global_index = mesh.indices[corner].vertex_index;
                    if (global_index < 0) {
                        continue; // 无点索引的角点（理论上 OBJ 面不会缺点索引）
                    }
                    auto [it, inserted] = global_to_local.try_emplace(global_index,
                        static_cast<Index>(mesh_data->vertex_positions_.size()));
                    if (inserted) {
                        mesh_data->vertex_positions_.push_back({ attrib.vertices[3 * global_index + 0],
                            attrib.vertices[3 * global_index + 1],
                            attrib.vertices[3 * global_index + 2] });
                    }
                    mesh_data->face_vertices_.push_back(it->second);
                }
                mesh_data->face_vertices_offset_.push_back(static_cast<Index>(mesh_data->face_vertices_.size()));
            }
        }
        mesh_data->vertex_count_ = static_cast<Index>(mesh_data->vertex_positions_.size());

        auto component = std::make_unique<ComponentData>();
        component->id = -1; // 组件 id 由系统入池时分配
        const auto name = group_input.originalName(fragments.front()->name);
        component->name = name.empty()
            ? "Comp_" + std::to_string(result.size())
            : std::string(name);
        component->mesh = std::move(mesh_data);
        result.push_back(std::move(component));
    }

    return result;
}

namespace {
//! @brief 写出网格的点与面（面点索引 = 1-based 偏移 + 局部点索引）
void writeMeshData(const MeshData& mesh, std::ostream& os, Index v_offset_1based)
{
    for (size_t v = 0; v < mesh.vertex_positions_.size(); v++) {
        const auto& pos = mesh.vertex_positions_[v];
        os << fmt::format("v {} {} {}\n", pos[0], pos[1], pos[2]);
    }
    // Loop over faces
    if (mesh.face_vertices_offset_.size() >= 2) {
        const Index face_count = static_cast<Index>(mesh.face_vertices_offset_.size() - 1);
        for (Index f = 0; f < face_count; f++) {
            const Index a = mesh.face_vertices_offset_[f];
            const Index b = mesh.face_vertices_offset_[f + 1];
            if (a < 0 || b < a || b > static_cast<Index>(mesh.face_vertices_.size())) {
                continue; // 跳过越界的脏面
            }
            os << "f";
            for (Index vi = a; vi < b; vi++) {
                os << fmt::format(" {}", v_offset_1based + mesh.face_vertices_[vi]);
            }
            os << "\n";
        }
    }
}
} // namespace

void ObjMeshIO::saveToFile(const MeshData& mesh, std::ostream& os)
{
    if (!os) {
        spdlog::error("Failed to open output OBJ file stream.");
        return;
    }

    os << "# Exported by MeshData\n";
    writeMeshData(mesh, os, 1);
}

void ObjMeshIO::saveToFile(const ComponentDatas& components, std::ostream& os)
{
    std::vector<const ComponentData*> views;
    views.reserve(components.size());
    for (const auto& component : components) {
        views.push_back(component.get());
    }
    saveToFile(views, os);
}

void ObjMeshIO::saveToFile(const std::vector<const ComponentData*>& components, std::ostream& os)
{
    if (!os) {
        spdlog::error("Failed to open output OBJ file stream.");
        return;
    }

    os << "# Exported by MeshData\n";

    // OBJ 点索引为 1-based 且全文件连续，需跨组件累加偏移
    Index v_offset_1based = 1;
    size_t comp_index = 0;
    for (const auto& component : components) {
        if (!component || !component->mesh) {
            continue; // 跳过无网格的组件
        }
        const MeshData& mesh = *component->mesh;

        // 组件名作为 OBJ object 名，loadFromFile 读回时按 shape 拆回组件
        os << "o " << (component->name.empty() ? "component_" + std::to_string(comp_index) : component->name) << "\n";
        comp_index++;

        writeMeshData(mesh, os, v_offset_1based);
        v_offset_1based += static_cast<Index>(mesh.vertex_positions_.size());
    }
}
