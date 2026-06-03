#include "IncrementalMeshContext.h"

#include "GeometryData.h"
#include "GeometryRegistry.h"
#include "GeometrySubshapeIndex.h"

#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>

#include <set>
#include <stdexcept>
#include <string>

namespace {

int edgeTypeIndex()
{
    return GeometrySubshapeIndex::typeIndex(TopAbs_EDGE);
}

int faceTypeIndex()
{
    return GeometrySubshapeIndex::typeIndex(TopAbs_FACE);
}

} // namespace

IncrementalMeshContext::IncrementalMeshContext(GeometryData& geometry, GeometryRegistry& registry)
    : cad_index_(&geometry.cad_index)
    , registry_(&registry)
{
    geometry.ensureCadIndexBuilt(registry);

    // 计算每个 CAD face 的边集合，后续 Gmsh 单面匹配只查这个拓扑缓存。
    const auto& faceMap = cad_index_->type_maps[faceTypeIndex()];
    const auto& edgeMap = cad_index_->type_maps[edgeTypeIndex()];
    face_edge_infos_.resize(static_cast<std::size_t>(faceMap.Extent()));

    for (int faceIdx = 1; faceIdx <= faceMap.Extent(); ++faceIdx) {
        auto face = TopoDS::Face(faceMap(faceIdx));
        std::set<GeomEdgeId> seen;

        for (TopExp_Explorer ex(face, TopAbs_EDGE); ex.More(); ex.Next()) {
            int localEdgeId = edgeMap.FindIndex(ex.Current());
            GeomEdgeId gid = cad_index_->edgeGlobalId(localEdgeId);
            if (gid != kInvalidGeomEdgeId && seen.insert(gid).second)
                face_edge_infos_[static_cast<std::size_t>(faceIdx - 1)].push_back({ gid, localEdgeId });
        }
    }
}

IncrementalMeshContext::~IncrementalMeshContext() = default;

int IncrementalMeshContext::globalEdgeCount() const
{
    return cad_index_->type_maps[edgeTypeIndex()].Extent();
}

std::vector<FaceEdgeInfo> IncrementalMeshContext::getFaceEdgeInfos(const TopoDS_Face& face) const
{
    int localFaceId = cad_index_->type_maps[faceTypeIndex()].FindIndex(face);
    if (localFaceId < 1 || static_cast<std::size_t>(localFaceId) > face_edge_infos_.size())
        return {};
    return face_edge_infos_[static_cast<std::size_t>(localFaceId - 1)];
}

std::vector<GeomEdgeId> IncrementalMeshContext::getFaceEdgeIds(const TopoDS_Face& face) const
{
    std::vector<GeomEdgeId> ids;
    for (const auto& info : getFaceEdgeInfos(face))
        ids.push_back(info.edgeId);
    return ids;
}

TopoDS_Edge IncrementalMeshContext::getEdgeByGlobalId(GeomEdgeId globalId) const
{
    const TopoDS_Shape* shape = registry_->getEdge(globalId);
    if (!shape)
        throw std::out_of_range("invalid CAD edge ID " + std::to_string(globalId));
    return TopoDS::Edge(*shape);
}

std::size_t IncrementalMeshContext::faceCount() const
{
    return static_cast<std::size_t>(cad_index_->type_maps[faceTypeIndex()].Extent());
}

TopoDS_Face IncrementalMeshContext::getFaceByIndex(std::size_t index) const
{
    int occIndex = static_cast<int>(index) + 1;
    const auto& faceMap = cad_index_->type_maps[faceTypeIndex()];
    if (occIndex < 1 || occIndex > faceMap.Extent())
        throw std::out_of_range("face index " + std::to_string(index) + " out of range");
    return TopoDS::Face(faceMap(occIndex));
}
