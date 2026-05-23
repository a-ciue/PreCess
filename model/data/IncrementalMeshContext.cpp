#include "IncrementalMeshContext.h"

#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>

#include <set>
#include <stdexcept>
#include <string>

struct IncrementalMeshContext::Impl {
    TopTools_IndexedMapOfShape globalEdgeMap;
    TopTools_IndexedMapOfShape globalFaceMap;
};

IncrementalMeshContext::IncrementalMeshContext(const TopoDS_Shape& shape)
    : impl_(std::make_unique<Impl>())
{
    TopExp::MapShapes(shape, TopAbs_EDGE, impl_->globalEdgeMap);
    TopExp::MapShapes(shape, TopAbs_FACE, impl_->globalFaceMap);
}

IncrementalMeshContext::~IncrementalMeshContext() = default;

int IncrementalMeshContext::globalEdgeCount() const
{
    return impl_->globalEdgeMap.Extent();
}

std::vector<int> IncrementalMeshContext::getFaceEdgeIds(const TopoDS_Face& face) const
{
    std::vector<int> ids;
    std::set<int> seen;
    for (TopExp_Explorer ex(face, TopAbs_EDGE); ex.More(); ex.Next()) {
        int gid = impl_->globalEdgeMap.FindIndex(ex.Current());
        if (gid > 0 && seen.insert(gid).second)
            ids.push_back(gid);
    }
    return ids;
}

TopoDS_Edge IncrementalMeshContext::getEdgeByGlobalId(int globalId) const
{
    if (globalId < 1 || globalId > impl_->globalEdgeMap.Extent())
        throw std::out_of_range("invalid edge ID " + std::to_string(globalId));
    return TopoDS::Edge(impl_->globalEdgeMap(globalId));
}

std::size_t IncrementalMeshContext::faceCount() const
{
    return static_cast<std::size_t>(impl_->globalFaceMap.Extent());
}

TopoDS_Face IncrementalMeshContext::getFaceByIndex(std::size_t index) const
{
    int occIndex = static_cast<int>(index) + 1;
    if (occIndex < 1 || occIndex > impl_->globalFaceMap.Extent())
        throw std::out_of_range("face index " + std::to_string(index) + " out of range");
    return TopoDS::Face(impl_->globalFaceMap(occIndex));
}
