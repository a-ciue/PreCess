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
}

IncrementalMeshContext::~IncrementalMeshContext() = default;

int IncrementalMeshContext::globalEdgeCount() const
{
    return cad_index_->type_maps[edgeTypeIndex()].Extent();
}

std::vector<GeomEdgeId> IncrementalMeshContext::getFaceEdgeIds(const TopoDS_Face& face) const
{
    std::vector<GeomEdgeId> ids;
    std::set<GeomEdgeId> seen;
    const auto& edgeMap = cad_index_->type_maps[edgeTypeIndex()];

    for (TopExp_Explorer ex(face, TopAbs_EDGE); ex.More(); ex.Next()) {
        int localEdgeId = edgeMap.FindIndex(ex.Current());
        GeomEdgeId gid = cad_index_->edgeGlobalId(localEdgeId);
        if (gid != kInvalidGeomEdgeId && seen.insert(gid).second)
            ids.push_back(gid);
    }
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
