#include "ComponentOperator.h"

#include "ComponentData.h"
#include "ModelLayer.h"
#include "MeshData.h"
#include "GeometryData.h"
#include "ModelData.h"

#include <TopoDS_Shape.hxx>

#include <stdexcept>
#include <utility>

ComponentOperator::ComponentOperator(Index component_id,
    ComponentData& component,
    ModelLayer& mgr,
    Index model_id) noexcept
    : component_id_(component_id)
    , model_id_(model_id)
    , component_(&component)
    , mgr_(&mgr)
{
}

const MeshData* ComponentOperator::mesh() const noexcept
{
    return component_ && component_->mesh ? component_->mesh.get() : nullptr;
}

GeometryData* ComponentOperator::geometry() const noexcept
{
    return component_ && component_->geometry ? component_->geometry.get() : nullptr;
}

Index ComponentOperator::modelId() const noexcept
{
    return model_id_;
}

ModelData* ComponentOperator::model() const
{
    return mgr_->modelById(model_id_);
}

MeshData& ComponentOperator::editableMesh(MeshEditKind kind)
{
    if (!component_ || !component_->mesh)
        throw std::runtime_error("ComponentOperator::editableMesh: component has no mesh");

    // 写必脏：获取可写入口即标脏（Topology 失效邻接懒表 + 记入待通知集合）
    mgr_->markComponentDirty(component_id_, kind);
    return *component_->mesh;
}

Index ComponentOperator::appendPoint(std::array<double, 3> pos)
{
    if (!component_ || !component_->mesh)
        throw std::runtime_error("ComponentOperator::appendPoint: component has no mesh");

    // 运行期加点原子四连：坐标、vertex_count_、gid 分配、gid 伴生表追加
    MeshData& mesh_data = *component_->mesh;
    const Index local_id = static_cast<Index>(mesh_data.vertex_positions_.size());
    mesh_data.vertex_positions_.push_back(pos);
    mesh_data.vertex_count_ = static_cast<Index>(mesh_data.vertex_positions_.size());
    component_->point_global_ids_.push_back(
        mgr_->pointIdMap().insert(component_id_, local_id));

    mgr_->markComponentDirty(component_id_, MeshEditKind::Topology);
    return local_id;
}

Index ComponentOperator::appendFace(const std::vector<Index>& local_point_ids)
{
    if (!component_ || !component_->mesh)
        throw std::runtime_error("ComponentOperator::appendFace: component has no mesh");
    if (local_point_ids.empty())
        throw std::invalid_argument("ComponentOperator::appendFace: empty point list");

    MeshData& mesh_data = *component_->mesh;
    for (Index local_id : local_point_ids) {
        if (local_id < 0 || local_id >= static_cast<Index>(mesh_data.vertex_positions_.size()))
            throw std::invalid_argument("ComponentOperator::appendFace: local point id out of range");
    }

    // 空 offset 数组先补 {0}，保持 offset 单调序列完整
    if (mesh_data.face_vertices_offset_.empty())
        mesh_data.face_vertices_offset_.push_back(0);

    const Index face_id = static_cast<Index>(mesh_data.face_vertices_offset_.size() - 1);
    mesh_data.face_vertices_.insert(
        mesh_data.face_vertices_.end(), local_point_ids.begin(), local_point_ids.end());
    mesh_data.face_vertices_offset_.push_back(static_cast<Index>(mesh_data.face_vertices_.size()));

    mgr_->markComponentDirty(component_id_, MeshEditKind::Topology);
    return face_id;
}

void ComponentOperator::replaceMesh(std::unique_ptr<MeshData> mesh)
{
    if (!mesh)
        throw std::invalid_argument("ComponentOperator::replaceMesh: null mesh");

    // 释放旧网格占用的点/边 gid
    if (component_->mesh) {
        component_->mesh_adjacency.releaseEdgeGlobalIds(mgr_->edgeIdMap());
        component_->releasePointGlobalIds(mgr_->pointIdMap());
    }

    // 新网格就位后按受控点纪律补分配点/边 gid（与 ModelLayer::addModel 的同步流程一致）
    mesh->vertex_count_ = static_cast<Index>(mesh->vertex_positions_.size());
    component_->mesh_adjacency.ensureEdgeGlobalIds(mgr_->edgeIdMap(), component_id_, *mesh);

    component_->mesh = std::move(mesh);
    component_->ensurePointGlobalIds(mgr_->pointIdMap());

    mgr_->markComponentDirty(component_id_, MeshEditKind::Topology);
}

Index ComponentOperator::materializeEdge(Index p0, Index p1)
{
    MeshData* mesh_data = component_ && component_->mesh ? component_->mesh.get() : nullptr;
    if (!mesh_data)
        throw std::runtime_error("ComponentOperator::materializeEdge: component has no mesh");
    if (p0 < 0 || p1 < 0 || p0 == p1)
        throw std::invalid_argument("ComponentOperator::materializeEdge: invalid endpoints");

    // 已物化则幂等返回既有 cell 序号
    if (auto edge = component_->mesh_adjacency.findEdgeByEndpoints(*mesh_data, p0, p1)) {
        const Index cell = component_->mesh_adjacency.edgeCellIndex(*mesh_data, *edge);
        if (cell >= 0)
            return cell;
    }

    const Index cell_index = static_cast<Index>(mesh_data->edge_vertices_.size() / 2);
    mesh_data->edge_vertices_.push_back(p0);
    mesh_data->edge_vertices_.push_back(p1);

    // 同步分配全局边 id
    component_->mesh_adjacency.ensureEdgeGlobalIds(mgr_->edgeIdMap(), component_id_, *mesh_data);

    // 标脏：失效邻接懒表，通知延迟到操作边界 flush
    mgr_->markComponentDirty(component_id_, MeshEditKind::Topology);
    return cell_index;
}

std::unique_ptr<ComponentData> ComponentOperator::takeSnapshot() const
{
    if (!component_)
        throw std::runtime_error("ComponentOperator::takeSnapshot: null component");
    return component_->clone();
}

void ComponentOperator::restoreSnapshot(const ComponentData& snapshot)
{
    if (!component_)
        throw std::runtime_error("ComponentOperator::restoreSnapshot: null component");

    // gid 对账：先释放现有点/边 gid 与旧几何索引，再按快照原值 reclaim
    component_->releasePointGlobalIds(mgr_->pointIdMap());
    component_->mesh_adjacency.releaseEdgeGlobalIds(mgr_->edgeIdMap());
    if (component_->geometry)
        component_->geometry->index.release(mgr_->geomRegistry()); // 防止 registry 残留旧 gid->shape

    component_->restoreFrom(snapshot);

    component_->reclaimPointGlobalIds(mgr_->pointIdMap());
    component_->mesh_adjacency.reclaimEdgeGlobalIds(mgr_->edgeIdMap(), component_id_);
    if (component_->geometry)
        component_->geometry->ensureIndexBuilt(mgr_->geomRegistry()); // gid 向量随快照保留，此处按原值 reclaim

    // 标脏：失效邻接懒表，通知延迟到操作边界 flush
    mgr_->markComponentDirty(component_id_, MeshEditKind::Topology);
}

Index ComponentOperator::appendGeometryShape(TopoDS_Shape shape)
{
    if (shape.IsNull())
        throw std::invalid_argument("Geometry shape is null");

    // 当前组件尚无有效几何时，直接用新形状初始化，不额外创建 Component。
    if (!component_->geometry)
        component_->geometry = std::make_unique<GeometryData>();
    if (!component_->geometry->rootShape || component_->geometry->rootShape->IsNull()) {
        if (component_->geometry->index.built)
            component_->geometry->index.release(mgr_->geomRegistry());
        component_->geometry->setRootShape(std::move(shape));
        component_->geometry->ensureIndexBuilt(mgr_->geomRegistry());
        mgr_->markComponentDirty(component_id_, MeshEditKind::Topology);
        return component_id_;
    }

    if (component_->mapping && !component_->mapping->empty())
        throw std::invalid_argument("Target component already contains geometry-mesh mapping");

    // 根形状改变后旧业务 ID 不再有效，必须释放并重新建立索引。
    component_->geometry->index.release(mgr_->geomRegistry());
    component_->geometry->appendRootShape(std::move(shape));
    component_->geometry->ensureIndexBuilt(mgr_->geomRegistry());
    mgr_->markComponentDirty(component_id_, MeshEditKind::Topology);
    return component_id_;
}

void ComponentOperator::removeMesh()
{
    if (!component_ || !component_->mesh)
        return;

    component_->releasePointGlobalIds(mgr_->pointIdMap());
    component_->mesh_adjacency.releaseEdgeGlobalIds(mgr_->edgeIdMap());
    component_->mesh.reset();

    // 标脏（顺带获得邻接失效），通知延迟到操作边界 flush
    mgr_->markComponentDirty(component_id_, MeshEditKind::Topology);
}

void ComponentOperator::removeGeometry()
{
    if (!component_ || !component_->geometry)
        return;

    component_->geometry->index.release(mgr_->geomRegistry());
    component_->geometry.reset();

    mgr_->markComponentDirty(component_id_, MeshEditKind::Topology);
}