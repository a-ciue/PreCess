/**
 * @file SessionQuery.h
 * @brief 会话层查询接口：模型层只读查询的原生（非 Qt）视图
 *
 * SessionQuery 将原 app 层 QModelQuery 的查询逻辑下沉为不依赖 Qt 的原生接口，
 * 返回类型化结构体而非 QVariantMap，供 QML 适配器（QModelQuery）与脚本绑定
 * 共用。经 ModelLayer 友元访问 models_ / component_to_model_ 私有表（友元身份
 * 自 QModelQuery 迁入，使 model 层不再感知 app 类名）。
 */
#ifndef SESSION_QUERY_H
#define SESSION_QUERY_H
#include "Core.h"
#include "GeometryDataVtk.h"
#include "Selection.h" // ElementEnum::Type

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class ModelLayer;

namespace session {

/** @brief 模型摘要（listModels 条目） */
struct ModelSummary {
    Index model_id { -1 };
    std::string name; //> 模型名称（UTF-8）
    int component_count { 0 };
};

/** @brief 组件摘要（componentSummaries 条目） */
struct ComponentSummary {
    Index component_id { -1 };
    std::string name;
    bool has_mesh { false };
    bool has_geometry { false };
    Index material_id { -1 };
};

/** @brief 网格规模摘要 */
struct MeshSummary {
    bool has_mesh { false };
    Index vertex_count { 0 };
    Index edge_count { 0 };
    Index face_count { 0 };
    Index solid_count { 0 };
};

/** @brief 几何拓扑规模摘要（ensureIndexBuilt 后统计） */
struct GeometrySummary {
    bool has_geometry { false };
    int vertex_count { 0 };
    int edge_count { 0 };
    int face_count { 0 };
    int solid_count { 0 };
};

/** @brief 组件属性渲染条目（原始名、展示名、类型与分量数） */
struct AttributeInfo {
    std::string name; //> 原始属性名（含 v_/e_/f_/s_ 前缀与分量数后缀）
    std::string display_name; //> 展示名（去除前缀与分量数后缀）
    ElementEnum::Type type { ElementEnum::Type::None };
    std::string type_name; //> 实体类型展示名（点/边/面/体）
    int attr_type { 0 }; //> 属性表序号（点 0、边 1、面 2、体 3）
    int component_count { 0 }; //> 分量数（按元组大小换算）
};

/**
 * @brief 模型层只读查询（CQRS 查询端，读写分离中的读视图）
 *
 * 生命周期上依赖宿主持有的 ModelLayer，自身不拥有数据。
 */
class SessionQuery {
public:
    explicit SessionQuery(ModelLayer& model);

    // —— 渲染数据视图 ——
    std::optional<MeshDataVtk> meshDataByComponent(Index component_id) const;
    //! @brief 模型内全部几何组件的渲染视图
    std::vector<GeometryDataVtk> geometryDataByModel(Index model_id) const;
    std::optional<GeometryDataVtk> geometryDataByComponent(Index component_id) const;

    // —— 身份与存在性 ——
    std::vector<Index> componentIds(Index model_id) const;
    //! @brief 模型内首个带网格组件的 id；无网格组件返回空（模型级查询的落点复用本查询）
    std::optional<Index> firstMeshComponentId(Index model_id) const;
    //! @brief 组件所属模型 id；未知组件返回 -1
    Index findModelIdByComponent(Index component_id) const;
    bool hasModel(Index model_id) const;
    bool hasComponent(Index component_id) const;
    //! @brief 模型名称；不存在返回空
    std::optional<std::string> modelName(Index model_id) const;
    //! @brief 组件名称；不存在返回空
    std::optional<std::string> componentName(Index component_id) const;
    //! @brief 组件内局部点 id -> 全局点 id（MeshIDMap gid）；越界或未分配返回 -1
    Index pointGlobalId(Index component_id, Index local_point_id) const;
    //! @brief 按两端点反查稳定局部边 id（跨拓扑编辑有效）；未命中返回空
    std::optional<Index> findEdgeByEndpoints(Index component_id, Index p0, Index p1) const;
    //! @brief 几何边映射的网格点 id 列表；无映射返回空
    std::vector<Index> geometryEdgeMappedPointIds(Index component_id, int local_geometry_edge_id) const;

    // —— 摘要查询 ——
    std::vector<ModelSummary> listModels() const;
    std::vector<ComponentSummary> componentSummaries(Index model_id) const;
    MeshSummary meshSummary(Index component_id) const;
    GeometrySummary geometrySummary(Index component_id) const;
    //! @brief 组件网格的全部属性渲染条目（四张属性表顺序遍历；属性名/类型查询复用本条目）
    std::vector<AttributeInfo> componentAttributeInfos(Index component_id) const;

    // —— 几何局部 id -> 全局 id ——
    std::optional<GeomFaceId> resolveGeometryFaceLocalId(Index component_id, int local_face_id) const;
    std::optional<GeomEdgeId> resolveGeometryEdgeLocalId(Index component_id, int local_edge_id) const;
    std::optional<GeomVertexId> resolveGeometryVertexLocalId(Index component_id, int local_vertex_id) const;
    std::optional<GeomSolidId> resolveGeometrySolidLocalId(Index component_id, int local_solid_id) const;

private:
    ModelLayer& model_;
};
}
#endif // SESSION_QUERY_H
