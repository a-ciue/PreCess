#include "GeometryInterferenceController.h"

#include "GeometryActor.h"
#include "GeometryActorManager.h"
#include "GeometryDataVtk.h"
#include "GeometryTopologyDiagnosticActor.h"
#include "GeometryTopologyEditor.h"
#include "QModelQuery.h"

#include <BRep_Builder.hxx>
#include <NCollection_DataMap.hxx>
#include <NCollection_Map.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>

#include <chrono>
#include <optional>
#include <spdlog/spdlog.h>
#include <string>
#include <utility>
#include <vector>

namespace {
//! 几何干涉检查不开其它诊断类别，这两个阈值只是接口要求，取任意正值。
constexpr double kInterferenceProbeEdgeLength = 1.0e-6;
constexpr double kInterferenceProbeFaceArea = 1.0e-12;

/** @brief 日志里标识一个 Model："id (名称)"，名称缺失时只留 id。 */
std::string modelLabel(QModelQuery& query, Index model_id)
{
    const QString name = query.getModelName(model_id);
    return std::to_string(model_id)
        + (name.isEmpty() ? std::string() : " (" + name.toStdString() + ")");
}
}

void GeometryInterferenceController::trackComponent(Index model_id, Index component_id)
{
    if (model_id >= 0)
        component_model_ids_[component_id] = model_id;
}

void GeometryInterferenceController::removeModel(
    Index model_id, GeometryActorManager& manager)
{
    for (auto it = component_model_ids_.begin(); it != component_model_ids_.end();) {
        if (it->second != model_id) {
            ++it;
            continue;
        }
        manager.deleteComponent(it->first);
        it = component_model_ids_.erase(it);
    }
    computed_model_ids_.erase(model_id);
}

void GeometryInterferenceController::removeComponent(
    Index component_id, GeometryActorManager& manager, QModelQuery& query)
{
    const auto model_it = component_model_ids_.find(component_id);
    const Index model_id = model_it == component_model_ids_.end() ? -1 : model_it->second;
    component_model_ids_.erase(component_id);
    manager.deleteComponent(component_id);

    if (model_id < 0)
        return;
    computed_model_ids_.erase(model_id);
    if (enabled_)
        rebuild(model_id, manager, query);
}

void GeometryInterferenceController::modelChanged(
    Index model_id, GeometryActorManager& manager, QModelQuery& query)
{
    if (model_id < 0)
        return;
    computed_model_ids_.erase(model_id);
    if (enabled_)
        rebuild(model_id, manager, query);
}

void GeometryInterferenceController::setEnabled(
    bool enabled, GeometryActorManager& manager, QModelQuery& query)
{
    enabled_ = enabled;
    if (!enabled_)
        return;

    // Component 归属表直接提供当前 Model 集合，避免维护重复的 Model 列表。
    std::unordered_set<Index> model_ids;
    for (const auto& [component_id, model_id] : component_model_ids_)
        model_ids.insert(model_id);
    for (Index model_id : model_ids) {
        if (computed_model_ids_.count(model_id) == 0)
            rebuild(model_id, manager, query);
    }
}

void GeometryInterferenceController::rebuild(
    Index model_id, GeometryActorManager& manager, QModelQuery& query)
{
    std::vector<Index> component_ids;
    NCollection_DataMap<TopoDS_Shape, Index, TopTools_ShapeMapHasher> face_owners;
    BRep_Builder builder;
    TopoDS_Compound root;
    builder.MakeCompound(root);

    for (Index component_id : query.getComponentIds(model_id)) {
        const std::optional<GeometryDataVtk> geometry
            = query.getGeometryVtkDataByComponent(component_id);
        if (!geometry.has_value() || geometry->shape.IsNull())
            continue;
        component_ids.push_back(component_id);
        builder.Add(root, geometry->shape);
        for (TopExp_Explorer face(geometry->shape, TopAbs_FACE); face.More(); face.Next()) {
            if (!face_owners.IsBound(face.Current()))
                face_owners.Bind(face.Current(), component_id);
        }
    }

    // 无论是否命中，先清空旧结果，避免 Component 残留上一次的标记。
    for (Index component_id : component_ids) {
        if (std::shared_ptr<GeometryActor> actor = manager.getComponentActor(component_id))
            actor->topologyDiagnostics().setInterferingFaces({ });
    }
    if (component_ids.empty()) {
        computed_model_ids_.insert(model_id);
        return;
    }

    GeometryTopologyDiagnosticOptions options {
        false, false, false, false, false, true, false
    };
    const auto started = std::chrono::steady_clock::now();
    const GeometryTopologyDiagnosticResult result = GeometryTopologyEditor::diagnoseTopology(
        root, kInterferenceProbeEdgeLength, kInterferenceProbeFaceArea, options);
    const double elapsed_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started)
                                  .count();

    NCollection_Map<TopoDS_Shape, TopTools_ShapeMapHasher> hit_faces;
    std::unordered_map<Index, std::vector<TopoDS_Face>> component_faces;
    for (const GeometryIntersectingFacePair& pair : result.interfering_face_pairs) {
        hit_faces.Add(pair.first);
        hit_faces.Add(pair.second);
        for (const TopoDS_Face& face : { pair.first, pair.second }) {
            if (face_owners.IsBound(face))
                component_faces[face_owners.Find(face)].push_back(face);
        }
    }

    spdlog::info("几何拓扑诊断（model {}）：几何干涉 耗时 {:.1f} ms —— {} 对，涉及 {} 张面",
        modelLabel(query, model_id), elapsed_ms, result.interfering_face_pairs.size(),
        hit_faces.Extent());
    for (Index component_id : component_ids) {
        if (std::shared_ptr<GeometryActor> actor = manager.getComponentActor(component_id)) {
            actor->topologyDiagnostics().setInterferingFaces(
                std::move(component_faces[component_id]));
        }
    }
    computed_model_ids_.insert(model_id);
}
