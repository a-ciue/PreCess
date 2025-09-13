#include "CTMeshModel.h"
#include "MeshData.h"
#include "../../ToolMesh.h"

//! @brief 从.obj文件读取网格模型并解析分组信息
//! @param obj_file 输入的.obj文件路径
//! @return 加载并解析完成的网格对象
std::unique_ptr<MeshLib::CTMesh> read_obj_with_groups(const std::filesystem::path& obj_file);

void CTMeshModel::update(MeshData& mesh_data)
{
    using namespace std;

    // 构造点坐标数组 MeshData::vertex_positions
    auto& vertex_positions = mesh_data.vertex_positions;
    vertex_positions.clear(); // 清空之前的顶点数据
    vertex_positions.reserve(mesh_->numVertices()); // 预留空间以提高性能
    unordered_map<Index, Index> vertex_index_map; // 顶点 ID 到索引的映射
    for (MeshLib::CTMesh::MeshVertexIterator vi(mesh_.get()); !vi.end(); ++vi) {
        vertex_index_map[vi.value()->id()] = vertex_positions.size();
        const CPoint& point = vi.value()->point();
        vertex_positions.emplace_back(array { point[0], point[1], point[2] });
    }

    // MeshData包括的patch id
    unordered_set<int> data_patch_ids;
    for (const auto& patch : mesh_data.patches_) {
        data_patch_ids.insert(patch.first);
    }

    // 按g将面分组
    std::unordered_map<int, std::vector<MeshLib::CTMesh::CFace*>> patch_faces;
    for (auto& face : mesh_->faces()) {
        int face_patch_id = face.get_g();
        patch_faces[face_patch_id].push_back(&face);
    }

    // 遍历每个组更新面
    mesh_data.face_vertices.clear();
    mesh_data.face_vertex_offsets = { 0 };
    mesh_data.face_vertex_offsets.reserve(mesh_->numFaces());
    for (const auto& [patch_id, faces] : patch_faces) {
        // 初始化 patches_[patch_id]
        auto& patch = mesh_data.patches_[patch_id];
        if (!patch) {
            // 新增patch需要判断是否需要新增Block，默认block id为patch_id
            auto& block = mesh_data.blocks_[patch_id];
            if (!block) {
                block = std::make_unique<Block>();
                block->id = patch_id;
            }
            block->patchIDs.insert(patch_id);

            patch = std::make_unique<Patch>(patch_id, patch_id);
        }

        // 从数据中移除已处理的patch id
        data_patch_ids.erase(patch_id); 

        // 遍历面更新：MeshData::face_vertices, Patch::faces
        patch->faces.clear(); // 清空之前的面片信息
        patch->faces.reserve(faces.size()); // 预留空间以提高性能
        for (auto& face : faces) {
            patch->faces.emplace_back(mesh_data.face_vertex_offsets.size() - 1); // 存面索引

            int i = 0;
            // 添加新面的点
            for (MeshLib::CTMesh::FaceVertexIterator vi(face); !vi.end(); ++vi) {
                auto& cur_index = mesh_data.face_vertices.emplace_back();
                cur_index = vertex_index_map[vi.value()->id()]; // 存点索引
                ++i;
            }

            mesh_data.face_vertex_offsets.push_back(i + mesh_data.face_vertex_offsets.back());
        }
    }

    // 处理MeshData没有被更新的 Patch，应该被删除
    for (const auto& patch_id : data_patch_ids) {
        if (mesh_data.patches_.count(patch_id)) {
            mesh_data.patches_.erase(patch_id);
        }
    }

    // 维护Block
    for (auto& [block_id, block] : mesh_data.blocks_)
    {
        // Block只存现有Patch
        for (auto& cur_patch : block->patchIDs)
        {
            if (!patch_faces.count(cur_patch))
            {
                block->patchIDs.erase(cur_patch);
            }
        }
        
        if (block->patchIDs.empty())
        {
            mesh_data.blocks_.erase(block_id);
        }
    }
}

void CTMeshModel::update(MeshData& mesh_data, const std::unordered_set<Index>& patch_ids)
{
}

CTMeshModel::CTMeshModel(const std::filesystem::path& mesh_path, Type type)
{
    switch (type)
    {
    case CTMeshModel::Type::OBJ:
		mesh_ = read_obj_with_groups(mesh_path);
        break;
    case CTMeshModel::Type::M:
		mesh_ = std::make_unique<MeshLib::CTMesh>();
		mesh_->read_m(mesh_path.string().c_str());
        break;
    default:
        break;
    }
}

CTMeshModel::~CTMeshModel() = default;

std::unique_ptr<MeshLib::CTMesh> read_obj_with_groups(const std::filesystem::path& obj_file) {
    if (!std::filesystem::exists(obj_file)) {
        throw std::runtime_error("OBJ file does not exist: " + obj_file.string());
    }

    // 第一次读取：加载网格几何信息
    auto mesh = std::make_unique<MeshLib::CTMesh>();
    // 调用 CTMesh 的 read_obj 成员函数读取网格
    mesh->read_obj(obj_file.string().c_str());
    // 第二次读取：解析分组信息并更新面片的 m_g 属性
    std::ifstream obj_stream(obj_file);
    if (!obj_stream.is_open()) {
        throw std::runtime_error("Failed to open OBJ file for reading groups: " + obj_file.string());
    }

    if (!mesh) {
        throw std::runtime_error("Failed to load mesh geometry from OBJ file.");
    }

    std::string line;
    int current_group_id = -1; // 当前分组的 ID
    // 获取面片集合并进行迭代
    auto face_iter = MeshLib::CTMesh::MeshFaceIterator(mesh.get());

    while (std::getline(obj_stream, line)) {
        std::istringstream line_stream(line);
        std::string prefix;
        line_stream >> prefix;

        // 处理 g 标签
        if (prefix == "g") {
            std::string group_name;
            line_stream >> group_name;

            // 为每个分组分配唯一的 ID
            current_group_id = std::stoi(group_name);
            std::cout << "Patch: " << group_name << " -> ID: " << current_group_id << std::endl;
        }
        // 处理面信息
        else if (prefix == "f" && !face_iter.end()) {
            // 为当前面设置分组 ID
            auto face = *face_iter;
            //face->set_g(current_group_id);  // 假设 `set_g` 更新 m_g 属性
            face->get_g() = current_group_id;
            ++face_iter;
        }
    }

    // 检查是否所有面都被分组
    if (!face_iter.end()) {
        throw std::runtime_error("Mismatch between OBJ face count and mesh faces.");
    }

    std::cout << "Finished reading OBJ file with groups. Total groups: " << (current_group_id + 1) << std::endl;
    return mesh;
}