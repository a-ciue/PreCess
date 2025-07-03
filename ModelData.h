/**
 * @file ModelData.h
 *
 * @brief 负责管理和操作网格模型数据的核心类
 *
 * ModelData 类用于存储和处理网格模型数据，包括面（Patch）、块（Block）和组（Group）的管理。
 * 它提供了一系列函数用于网格操作，如网格划分、合并和重划分等，同时维护与 MeshActor 之间的关联，
 * 以便进行可视化和渲染。
 *
 * @author 徐昊阳 haoyangxu06\@gmail.com
 * @date 2025/3/8
 */
#ifndef MODEL_H
#define MODEL_H
#include <memory>
#include <unordered_map>
#include <unordered_set>

#include "MeshData.h"
#include "SplineData.h"
#include "Selection.h"

//! @brief Model主要负责处理模型数据，先更新模型数据，再更新ModelActor调函数
/**
*@brief 负责管理和操作网格模型数据的核心类
*
* ModelData 负责管理网格数据，包括 Patch、Block 和 Group 的存储、更新和操作。
* 该类提供了网格划分、合并、重划分等功能，并维护与 MeshActor 之间的关联，
* 以便进行可视化和渲染。
*/
class ModelData {
public:
    enum class Type { Mesh, Spline };

    /* ============ 构造（仅声明） ============ */
    explicit ModelData(MeshData mesh);
    explicit ModelData(SplineData spline);

    /* ============ 类型查询 ============ */
    Type type() const;
    bool isMesh()   const noexcept;
    bool isSpline() const noexcept;

    /* ============ 访问器 ============ */
    MeshData* asMeshData() noexcept;
    const MeshData* asMeshData() const noexcept;

    SplineData* asSplineData() noexcept;
    const SplineData* asSplineData() const noexcept;

    /* ============ 通用 visit （模板，必须放头文件） ============ */
    template<typename Visitor>
    decltype(auto) visit(Visitor&& v) {
        return std::visit(std::forward<Visitor>(v), data_);
    }

private:

    //! @brief 输出网格文件，选择面输出（不带组信息）、块输出、组输出
    //! @param mesh_path 输出文件路径
    //! @param mode 选定输出模式
    //! @param extension 输出文件拓展名
    void write_mesh(const std::filesystem::path& mesh_path, ModelRenderMode mode, const QString &extension);

    
    //! @brief 根据给定id找到mesh的face，进行面分割
    //! @param patch_id 面所在的patch
    //! @param face_id 在该patch上的face id
    void split_face(QSelection* selection);

    //! @brief 根据给定id找到mesh的edge，进行边分割
    //! @param patch_id 边所在的patch
    //! @param edge_v_id1 其中一个边点id
    //! @param edge_v_id2 另一个边点id
    void split_edge(QSelection* selection);
    
    //! @brief 合并给定block，并更新block actor，依赖ModelActor
    //! @param block_ids
    void merge_blocks(QSelection* selection);

    //! @brief 合并给定group，并更新group actor，依赖ModelActor
    //! @param group_ids
    void merge_groups(QSelection* selection);
    
    //! @brief remesh指定block，依赖MeshUtil、update_patches、update_actors
    void remesh_block(QSelection* selection);

    //! @brief remesh指定group，依赖MeshUtil、update_patches、update_actors
    void remesh_group(QSelection* selection);

    /**
     * @brief 获取指定面 (face) 所属的 patch ID
     *
     * @param face_id 需要查询的面 ID
     * @return int 该面所属的 patch ID
     */
    int face_patch_id(int face_id);

    /**
     * @brief 获取指定 patch 内所有面的 ID
     *
     * @param patch_id 需要查询的 patch ID
     * @return const std::vector<int>& 该 patch 内包含的所有面 ID
     */
    const std::vector<int>& patch_face_ids(int patch_id);

    /**
     * @brief 获取指定 patch 内所有顶点的 ID
     *
     * @param patch_id 需要查询的 patch ID
     * @return const std::vector<int>& 该 patch 内包含的所有顶点 ID
     */
    const std::vector<int>& patch_vertex_ids(int patch_id);

    /**
     * @brief 获取指定 patch 所属的 block ID
     *
     * @param patch_id 需要查询的 patch ID
     * @return int 该 patch 所属的 block ID
     */
    int patch_block_id(int patch_id);
    //const std::vector<int>& block_patch_ids(int block_id);

    /**
     * @brief 获取指定 block 所属的 group ID
     *
     * @param patch_id 需要查询的 patch ID
     * @return int 该 block 所属的 group ID
     */
    int block_group_id(int patch_id);
    //const std::vector<int>& group_block_ids(int group_id);


    Index getId() const {
        if (type_ == Type::Mesh){
            return id_;
        }
    }

    /**
     * @brief 获取模型名称
     *
     * @return QString 当前模型的名称
     */
    //QString getModelName() const { return model_name_; }
    QString getModelName() const{
        switch(type_) {
            case Type::Mesh:
                return std::get<MeshData>(data_).model_name_;       // MeshData.id
            case Type::Spline:
                return std::get<SplineData>(data_).model_name_;     // 假设你在 SplineData 里也有 id
        }
        throw std::logic_error("Unknown model type");
    }

    /**
     * @brief 设置模型名称
     *
     * @param name 要设置的模型名称
     */
    //void setModelName(const QString& name) { model_name_ = name; }
    void setModelName(const QString& name){
        if (isMesh())
            asMeshData()->model_name_ = name;
        else
            asSplineData()->model_name_ = name;
    }

    /**
     * @brief 获取渲染模型需要的数据
     * @return 模型数据
     */
    MeshDataVtk getModelData();

    std::optional<SplineDataVtk> getSplineData();

    //! @brief 根据CToolFace::m_g()为面所在patch，读取mesh_更新指定patch的patches
    void update_patches(const std::vector<int>& patch_ids, bool new_patch = true);
    void update_patches(const std::unordered_set<int>& patch_ids, bool new_patch = true);

    //! @brief 更新指定patch的actor
    void update_actors(const std::vector<int>& patch_ids);

    Type                                   type_;
    std::variant<MeshData, SplineData>     data_;

    Index id_{ -1 }; //!< 模型的唯一标识符

    friend class QModelQuery;          //!< 声明 QModelQuery 为友元，以允许其访问 ModelData 私有数据
    friend class TestModel;           //!< 声明 TestModel 为友元，用于GoogleTest
    friend class ModelOperator;    //!< 声明 ModelOperator 为友元，以允许其访问 ModelData 私有数据
    friend class FileHandler; //!< 声明 FileHandler 为友元，以允许其访问 ModelData 私有数据
    friend class ModelManager; //!< 声明 ModelManager 为友元，以允许其访问 ModelData 私有数据
};
#endif // MODEL_H