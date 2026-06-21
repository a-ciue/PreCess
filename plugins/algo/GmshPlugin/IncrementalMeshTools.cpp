#include "IncrementalMeshTools.h"
#include "IncrementalMeshContext.h"

#include "GeometryRegistry.h"
#include "GmshMeshTypes.h"
#include "MeshData.h"
#include "ModelLayer.h"

#include <BRepGProp.hxx>
#include <BRep_Builder.hxx>
#include <GProp_GProps.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <STEPControl_Reader.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>

#include <gmsh.h>
#include <spdlog/spdlog.h>

#include <cmath>
#include <map>
#include <vector>
#include <fstream>
#include <unordered_map>

namespace {

// 合并网格时的顶点去重
class TempNodeLookup {
public:
    explicit TempNodeLookup(
        MeshData& mesh_data,
        GmshIncrementalMeshState& state,
        ModelLayer& model_layer,
        double tolerance = 1e-7)
        : _mesh_data(mesh_data)
        , _state(state)
        , _model_layer(model_layer)
        , _tolerance(tolerance)
    {
        const auto& vertices = _mesh_data.vertex_positions_;
        const auto& global_ids = _state.local_to_global_point_ids;
        for (size_t i = 0; i < vertices.size(); ++i) {
            auto qc = _quantize(vertices[i][0], vertices[i][1], vertices[i][2]);
            if (i < global_ids.size()) {
                _map[qc] = global_ids[i];
            }
        }
    }

    Index getOrInsert(double x, double y, double z)
    {
        auto qc = _quantize(x, y, z);
        auto it = _map.find(qc);
        if (it != _map.end())
            return it->second;

        std::array<double, 3> point { x, y, z };
        Index global_id = _model_layer.appendGlobalPoints({ point });
        if (_mesh_data.global_point_base_ < 0) {
            _mesh_data.global_point_base_ = global_id;
        }
        _mesh_data.vertex_positions_.push_back(point);
        _mesh_data.vertex_count_ = static_cast<Index>(_mesh_data.vertex_positions_.size());
        _mesh_data.point_ids_are_global_ = true;
        _state.local_to_global_point_ids.push_back(global_id);
        _map[qc] = global_id;
        return global_id;
    }

private:
    struct QuantizedCoord {
        int64_t ix, iy, iz;
        bool operator==(const QuantizedCoord& o) const
        {
            return ix == o.ix && iy == o.iy && iz == o.iz;
        }
    };

    struct CoordHash {
        size_t operator()(const QuantizedCoord& c) const
        {
            size_t h = 0;
            h ^= std::hash<int64_t>()(c.ix) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<int64_t>()(c.iy) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<int64_t>()(c.iz) + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };

    QuantizedCoord _quantize(double x, double y, double z) const
    {
        return {
            static_cast<int64_t>(std::round(x / _tolerance)),
            static_cast<int64_t>(std::round(y / _tolerance)),
            static_cast<int64_t>(std::round(z / _tolerance))
        };
    }

    MeshData& _mesh_data;
    GmshIncrementalMeshState& _state;
    ModelLayer& _model_layer;
    double _tolerance;
    std::unordered_map<QuantizedCoord, Index, CoordHash> _map;
};

// ---- 加载 STEP ----
TopoDS_Shape loadStep(const std::string& path)
{
    STEPControl_Reader reader;
    if (reader.ReadFile(path.c_str()) != IFSelect_RetDone) {
        spdlog::error("Cannot read STEP: {}", path);
        return {};
    }
    reader.TransferRoots();
    return reader.OneShape();
}

// ---- 包 Compound ----
TopoDS_Compound makeFaceCompound(const TopoDS_Face& face)
{
    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    builder.Add(compound, face);
    return compound;
}

// 将前端 Combo 索引转换为内部曲面网格类型。
GmshSurfaceMeshType parseSurfaceMeshType(int meshTypeIndex)
{
    if (meshTypeIndex == static_cast<int>(GmshSurfaceMeshType::QuadDominant))
        return GmshSurfaceMeshType::QuadDominant;
    if (meshTypeIndex == static_cast<int>(GmshSurfaceMeshType::StructuredQuadrilateral))
        return GmshSurfaceMeshType::StructuredQuadrilateral;
    if (meshTypeIndex != static_cast<int>(GmshSurfaceMeshType::Triangle))
        spdlog::warn("GmshMesh: invalid mesh type {}, using triangle", meshTypeIndex);
    return GmshSurfaceMeshType::Triangle;
}

// 返回日志中使用的曲面网格类型名称。
const char* surfaceMeshTypeName(GmshSurfaceMeshType meshType)
{
    if (meshType == GmshSurfaceMeshType::QuadDominant)
        return "quad-dominant";
    if (meshType == GmshSurfaceMeshType::StructuredQuadrilateral)
        return "structured-quad";
    return "triangle";
}

// 逐条导入当前面的 OCC 边，并直接使用公开 API 返回的 tag 建立 CAD 边映射。
// 后续导入整个面时，Gmsh 会通过内部 Shape 映射复用这些已经绑定的曲线 tag。
std::map<int, GeomEdgeId> importGmshEdges(const TopoDS_Face& face,
    const IncrementalMeshContext& ctx)
{
    std::map<int, GeomEdgeId> result;
    const auto edgeInfos = ctx.getFaceEdgeInfos(face);
    for (const auto& info : edgeInfos) {
        TopoDS_Edge edge = ctx.getEdgeByGlobalId(info.edgeId);

        gmsh::vectorpair edgeDimTags;
        gmsh::model::occ::importShapesNativePointer(
            static_cast<const void*>(&edge), edgeDimTags);

        int gmshTag = 0;
        for (const auto& [dim, tag] : edgeDimTags) {
            if (dim == 1) {
                gmshTag = tag;
                break;
            }
        }

        if (gmshTag <= 0) {
            spdlog::warn("  Cannot import OCC edge {}", info.edgeId);
            continue;
        }

        result[gmshTag] = info.edgeId;
        spdlog::info("  GMSH edge {} -> OCC edge {}", gmshTag, info.edgeId);
    }

    spdlog::info("  Imported {}/{} OCC edges",
        result.size(), edgeInfos.size());
    return result;
}

// 保存结构化划分中一条边的节点数，以及该数量是否由共享边缓存固定。
struct EdgeTransfiniteInfo {
    int gmshTag {};
    int pointCount {};
    bool fixedByExistingMesh {};
};

// 根据曲线长度和目标网格尺寸估算节点数，并延续原实现的偶数节点约束。
int estimateEdgePointCount(int gmshTag, double meshSize)
{
    double length = 0.0;
    gmsh::model::occ::getMass(1, gmshTag, length);

    int pointCount = static_cast<int>(std::ceil(length / meshSize)) + 2;
    if (pointCount < 2)
        pointCount = 2;
    if (pointCount % 2 == 1)
        ++pointCount;
    return pointCount;
}

// 优先采用已有共享边节点数，否则根据曲线长度和网格尺寸估算。
EdgeTransfiniteInfo makeEdgeTransfiniteInfo(
    int gmshTag,
    const std::map<int, GeomEdgeId>& gmshToOcc,
    const GmshIncrementalMeshState& state,
    double meshSize)
{
    EdgeTransfiniteInfo info;
    info.gmshTag = gmshTag;

    auto occIt = gmshToOcc.find(gmshTag);
    if (occIt != gmshToOcc.end()) {
        auto cacheIt = state.meshedEdgesCache.find(occIt->second);
        if (cacheIt != state.meshedEdgesCache.end() && !cacheIt->second.empty()) {
            info.pointCount = static_cast<int>(cacheIt->second.nodeCount());
            info.fixedByExistingMesh = true;
            return info;
        }
    }

    info.pointCount = estimateEdgePointCount(gmshTag, meshSize);
    return info;
}

// 协调一对相对边的节点数；两条共享边节点数不一致时拒绝结构化划分。
bool resolveOppositeEdgePointCount(
    const EdgeTransfiniteInfo& first,
    const EdgeTransfiniteInfo& opposite,
    int& pointCount)
{
    if (first.fixedByExistingMesh && opposite.fixedByExistingMesh) {
        if (first.pointCount != opposite.pointCount) {
            spdlog::warn("GmshMesh: structured quad rejected, opposite edges {} and {} have {} / {} points",
                first.gmshTag, opposite.gmshTag,
                first.pointCount, opposite.pointCount);
            return false;
        }
        pointCount = first.pointCount;
        return true;
    }

    if (first.fixedByExistingMesh) {
        pointCount = first.pointCount;
        return true;
    }
    if (opposite.fixedByExistingMesh) {
        pointCount = opposite.pointCount;
        return true;
    }

    pointCount = static_cast<int>((first.pointCount + opposite.pointCount) * 0.5 + 0.5);
    if (pointCount < 2)
        pointCount = 2;
    if (pointCount % 2 == 1)
        ++pointCount;
    return true;
}

// 使用 Gmsh 公开 API 配置四边形主导或结构化四边形约束。
bool configureSurfaceMeshType(
    int faceTag,
    GmshSurfaceMeshType meshType,
    const std::map<int, GeomEdgeId>& gmshToOcc,
    const GmshIncrementalMeshState& state,
    double meshSize)
{
    if (meshType == GmshSurfaceMeshType::Triangle)
        return true;

    if (meshType == GmshSurfaceMeshType::QuadDominant) {
        gmsh::model::mesh::setRecombine(2, faceTag, 45.0);
        return true;
    }

    gmsh::vectorpair boundary;
    gmsh::model::getBoundary({ { 2, faceTag } }, boundary, false, false, false);

    std::vector<int> edgeTags;
    for (const auto& [dim, tag] : boundary) {
        if (dim == 1)
            edgeTags.push_back(std::abs(tag));
    }
    if (edgeTags.size() != 4) {
        spdlog::warn("GmshMesh: structured quad requires 4 boundary edges, surface {} has {}",
            faceTag, edgeTags.size());
        return false;
    }

    std::array<EdgeTransfiniteInfo, 4> edges {};
    for (std::size_t i = 0; i < edges.size(); ++i)
        edges[i] = makeEdgeTransfiniteInfo(edgeTags[i], gmshToOcc, state, meshSize);

    int firstPairPointCount = 0;
    int secondPairPointCount = 0;
    if (!resolveOppositeEdgePointCount(edges[0], edges[2], firstPairPointCount))
        return false;
    if (!resolveOppositeEdgePointCount(edges[1], edges[3], secondPairPointCount))
        return false;

    gmsh::model::mesh::setTransfiniteCurve(edges[0].gmshTag, firstPairPointCount);
    gmsh::model::mesh::setTransfiniteCurve(edges[2].gmshTag, firstPairPointCount);
    gmsh::model::mesh::setTransfiniteCurve(edges[1].gmshTag, secondPairPointCount);
    gmsh::model::mesh::setTransfiniteCurve(edges[3].gmshTag, secondPairPointCount);
    gmsh::model::mesh::setTransfiniteSurface(faceTag);
    gmsh::model::mesh::setRecombine(2, faceTag, 45.0);
    return true;
}

// ---- 注入约束边 ----
bool injectConstrainedEdge(int gmshTag,
    const MeshedEdgeData& nodes,
    std::size_t& nodeCounter,
    std::size_t& elemCounter,
    std::map<int, std::size_t>& vtxNodeMap)
{
    if (nodes.empty())
        return false;
    std::size_t nc = nodes.nodeCount();

    std::vector<std::pair<int, int>> vtxBnd;
    gmsh::model::getBoundary({ { 1, gmshTag } }, vtxBnd, false, false, false);
    if (vtxBnd.size() < 2)
        return false;

    int vtx0 = std::abs(vtxBnd[0].second);
    int vtx1 = std::abs(vtxBnd[1].second);

    double gx0, gy0, gz0;
    {
        double a, b, c, d, e, f;
        gmsh::model::getBoundingBox(0, vtx0, a, b, c, d, e, f);
        gx0 = a;
        gy0 = b;
        gz0 = c;
    }

    double distFwd = std::sqrt(
        std::pow(gx0 - nodes.coords[0], 2) + std::pow(gy0 - nodes.coords[1], 2) + std::pow(gz0 - nodes.coords[2], 2));
    bool reversed = (distFwd > 1e-6);

    auto orderedCoords = nodes.coords;
    auto orderedParams = nodes.paramCoords;

    if (reversed) {
        std::vector<double> rev(orderedCoords.size());
        for (std::size_t i = 0; i < nc; ++i) {
            std::size_t ri = nc - 1 - i;
            rev[i * 3] = orderedCoords[ri * 3];
            rev[i * 3 + 1] = orderedCoords[ri * 3 + 1];
            rev[i * 3 + 2] = orderedCoords[ri * 3 + 2];
        }
        orderedCoords = rev;
        orderedParams.clear();
        for (std::size_t i = 1; i + 1 < nc; ++i) {
            try {
                std::vector<double> cc, cp;
                gmsh::model::getClosestPoint(1, gmshTag,
                    { orderedCoords[i * 3], orderedCoords[i * 3 + 1], orderedCoords[i * 3 + 2] }, cc, cp);
                if (!cp.empty())
                    orderedParams.push_back(cp[0]);
            } catch (...) {
                orderedParams.push_back(double(i) / double(nc - 1));
            }
        }
    }

    // 起点
    std::size_t tagV0;
    auto it0 = vtxNodeMap.find(vtx0);
    if (it0 != vtxNodeMap.end()) {
        tagV0 = it0->second;
    } else {
        tagV0 = nodeCounter++;
        gmsh::model::mesh::addNodes(0, vtx0, { tagV0 },
            { orderedCoords[0], orderedCoords[1], orderedCoords[2] });
        vtxNodeMap[vtx0] = tagV0;
    }

    // 终点
    std::size_t tagV1;
    std::size_t last = nc - 1;
    auto it1 = vtxNodeMap.find(vtx1);
    if (it1 != vtxNodeMap.end()) {
        tagV1 = it1->second;
    } else {
        tagV1 = nodeCounter++;
        gmsh::model::mesh::addNodes(0, vtx1, { tagV1 },
            { orderedCoords[last * 3], orderedCoords[last * 3 + 1], orderedCoords[last * 3 + 2] });
        vtxNodeMap[vtx1] = tagV1;
    }

    // 内部节点
    std::vector<std::size_t> innerTags;
    std::vector<double> innerCoords, innerParams;

    for (std::size_t i = 1; i + 1 < nc; ++i) {
        innerTags.push_back(nodeCounter++);
        innerCoords.push_back(orderedCoords[i * 3]);
        innerCoords.push_back(orderedCoords[i * 3 + 1]);
        innerCoords.push_back(orderedCoords[i * 3 + 2]);
    }

    if (orderedParams.size() == innerTags.size()) {
        innerParams = orderedParams;
    } else {
        innerParams.clear();
        for (std::size_t i = 0; i < innerTags.size(); ++i) {
            std::size_t ci = i + 1;
            try {
                std::vector<double> cc, cp;
                gmsh::model::getClosestPoint(1, gmshTag,
                    { orderedCoords[ci * 3], orderedCoords[ci * 3 + 1], orderedCoords[ci * 3 + 2] }, cc, cp);
                if (!cp.empty())
                    innerParams.push_back(cp[0]);
            } catch (...) {
                innerParams.push_back(double(i + 1) / double(nc - 1));
            }
        }
    }

    if (!innerTags.empty())
        gmsh::model::mesh::addNodes(1, gmshTag, innerTags, innerCoords, innerParams);

    // 线单元
    std::vector<std::size_t> allN;
    allN.push_back(tagV0);
    for (auto t : innerTags)
        allN.push_back(t);
    allN.push_back(tagV1);

    std::vector<std::size_t> eT, eN;
    for (std::size_t i = 0; i + 1 < allN.size(); ++i) {
        eT.push_back(elemCounter++);
        eN.push_back(allN[i]);
        eN.push_back(allN[i + 1]);
    }
    gmsh::model::mesh::addElementsByType(gmshTag, 1, eT, eN);
    return true;
}

// ---- 提取边节点 ----
MeshedEdgeData extractEdgeNodes(int gmshTag)
{
    MeshedEdgeData en;
    std::vector<std::pair<int, int>> vtxBnd;
    gmsh::model::getBoundary({ { 1, gmshTag } }, vtxBnd, false, false, false);
    if (vtxBnd.size() < 2)
        return en;

    {
        int vt = std::abs(vtxBnd[0].second);
        std::vector<std::size_t> nt;
        std::vector<double> co, pa;
        gmsh::model::mesh::getNodes(nt, co, pa, 0, vt, false, false);
        if (!nt.empty())
            en.coords.insert(en.coords.end(), co.begin(), co.begin() + 3);
    }

    {
        std::vector<std::size_t> it;
        std::vector<double> ic, ip;
        gmsh::model::mesh::getNodes(it, ic, ip, 1, gmshTag, false, true);
        en.coords.insert(en.coords.end(), ic.begin(), ic.end());
        en.paramCoords.insert(en.paramCoords.end(), ip.begin(), ip.end());
    }

    {
        int vt = std::abs(vtxBnd[1].second);
        std::vector<std::size_t> nt;
        std::vector<double> co, pa;
        gmsh::model::mesh::getNodes(nt, co, pa, 0, vt, false, false);
        if (!nt.empty())
            en.coords.insert(en.coords.end(), co.begin(), co.begin() + 3);
    }

    return en;
}

// ---- 提取面网格（局部索引）----
SingleFaceMeshResult extractFaceMesh(int faceTag)
{
    SingleFaceMeshResult result;
    result.face_vertices_offset.push_back(0);

    std::vector<std::size_t> nodeTags;
    std::vector<double> coords, paramCoords;
    gmsh::model::mesh::getNodes(nodeTags, coords, paramCoords, 2, faceTag, true, false);

    std::map<std::size_t, std::size_t> tagToLocal;
    for (size_t i = 0; i < nodeTags.size(); ++i) {
        tagToLocal[nodeTags[i]] = result.vertices.size();
        result.vertices.push_back({ coords[3 * i], coords[3 * i + 1], coords[3 * i + 2] });
    }

    std::vector<int> eTypes;
    std::vector<std::vector<std::size_t>> eTags, eNodes;
    gmsh::model::mesh::getElements(eTypes, eTags, eNodes, 2, faceTag);

    size_t cnt = 0;
    for (size_t t = 0; t < eTypes.size(); ++t) {
        if (eTypes[t] != 2 && eTypes[t] != 3)
            continue;
        int npe = (eTypes[t] == 2) ? 3 : 4;
        for (size_t i = 0; i < eTags[t].size(); ++i) {
            for (int j = 0; j < npe; ++j)
                result.face_vertices.push_back(tagToLocal[eNodes[t][i * npe + j]]);
            result.face_vertices_offset.push_back(result.face_vertices.size());
            cnt++;
        }
    }
    result.success = (cnt > 0);
    spdlog::info("  Extracted: {} nodes, {} elements", nodeTags.size(), cnt);
    return result;
}

// 将当前划分出的 Gmsh 边节点按全局 CAD 边 ID 缓存，供后续相邻面复用。
void storeNewEdges(GmshIncrementalMeshState& state, const std::map<int, GeomEdgeId>& g2o)
{
    int nNew = 0;
    int nShared = 0;
    for (auto& [gt, oid] : g2o) {
        if (state.meshedEdgeRefCounts.find(oid) == state.meshedEdgeRefCounts.end()) {
            auto ed = extractEdgeNodes(gt);
            if (!ed.empty()) {
                state.meshedEdgesCache[oid] = std::move(ed);
                state.meshedEdgeRefCounts[oid] = 1; // 首次创建
                nNew++;
            }
        } else {
            state.meshedEdgeRefCounts[oid]++; // 被另一个面复用
            nShared++;
        }
    }
    spdlog::info("GmshMesh:  {} new edges stored, {} edges reused (total cached: {})", nNew, nShared, state.meshedEdgeRefCounts.size());
}

void mergeMeshResult(
    MeshData& mesh_data,
    GmshIncrementalMeshState& state,
    ModelLayer& model_layer,
    const SingleFaceMeshResult& result)
{
    if (!result.success)
        return;

    if (mesh_data.face_vertices_offset_.empty())
        mesh_data.face_vertices_offset_.push_back(0);

    TempNodeLookup lookup(mesh_data, state, model_layer, 1e-7);

    std::vector<Index> localToGlobal(result.vertices.size());
    for (size_t i = 0; i < result.vertices.size(); ++i) {
        localToGlobal[i] = lookup.getOrInsert(
            result.vertices[i][0],
            result.vertices[i][1],
            result.vertices[i][2]);
    }

    for (size_t i = 0; i + 1 < result.face_vertices_offset.size(); ++i) {
        size_t start = result.face_vertices_offset[i];
        size_t end = result.face_vertices_offset[i + 1];
        for (size_t j = start; j < end; ++j) {
            mesh_data.face_vertices_.push_back(
                localToGlobal[result.face_vertices[j]]);
        }
        mesh_data.face_vertices_offset_.push_back(
            static_cast<Index>(mesh_data.face_vertices_.size()));
    }

    spdlog::info("GmshMesh: merged total {} nodes, {} cells",
        mesh_data.vertex_positions_.size(),
        mesh_data.face_vertices_offset_.size() - 1);
}

} // anonymous namespace

// 公开接口
bool IncrementalMeshTools::initMeshing(const std::string& stepFile,
    GeometryData& geometry,
    GmshIncrementalMeshState& state,
    GeometryRegistry& registry)
{
    state.clear();
    if (geometry.cad_index.built)
        geometry.cad_index.release(registry);

    geometry.rootShape = std::make_unique<TopoDS_Shape>(loadStep(stepFile));
    if (geometry.rootShape->IsNull()) {
        spdlog::error("Failed to load: {}", stepFile);
        return false;
    }
    state.meshContext = std::make_unique<IncrementalMeshContext>(geometry, registry);
    spdlog::info("Loaded: {} faces, {} global edges",
        state.meshContext->faceCount(),
        state.meshContext->globalEdgeCount());
    return state.meshContext->faceCount() > 0;
}

SingleFaceMeshResult IncrementalMeshTools::meshSingleFace(
    MeshData& mesh_data,
    GeometryData& geometry,
    GmshIncrementalMeshState& state,
    ModelLayer& model_layer,
    std::size_t faceIndex,
    double meshSize,
    int meshTypeIndex)
{
    SingleFaceMeshResult result;
    GmshSurfaceMeshType meshType = parseSurfaceMeshType(meshTypeIndex);

    if (!state.meshContext) {
        spdlog::error("meshContext null, call initMeshing first");
        return result;
    }
    if (faceIndex >= state.meshContext->faceCount()) {
        spdlog::error("faceIndex {} out of range", faceIndex);
        return result;
    }

    spdlog::info("=== Meshing face {}/{} (size={:.6f}, type={}) ===",
        faceIndex + 1, state.meshContext->faceCount(), meshSize,
        surfaceMeshTypeName(meshType));

    TopoDS_Face face = state.meshContext->getFaceByIndex(faceIndex);
    if (face.IsNull()) {
        spdlog::error("Face {} is null or invalid", faceIndex);
        return result;
    }
    TopoDS_Compound compound = makeFaceCompound(face);

    gmsh::initialize();
    gmsh::option::setNumber("General.Terminal", 1);
    gmsh::model::add("face_model");

    // 先逐边导入并记录返回 tag，再导入整个面；面导入会复用相同 OCC 边的 tag。
    auto gmshToOcc = importGmshEdges(face, *state.meshContext);

    gmsh::vectorpair outDimTags;
    gmsh::model::occ::importShapesNativePointer(
        static_cast<const void*>(&compound), outDimTags);
    gmsh::model::occ::synchronize();

    std::vector<std::pair<int, int>> faceDimTags;
    gmsh::model::getEntities(faceDimTags, 2);
    if (faceDimTags.empty()) {
        spdlog::error("  No face after import");
        gmsh::finalize();
        return result;
    }
    int faceTag = faceDimTags[0].second;

    std::size_t nodeCounter = 1, elemCounter = 1;
    int shared = 0, free = 0;
    std::map<int, std::size_t> vtxNodeMap;

    for (auto& [gt, oid] : gmshToOcc) {
        if (state.meshedEdgeRefCounts.count(oid) > 0) {
            injectConstrainedEdge(gt, state.meshedEdgesCache.at(oid),
                nodeCounter, elemCounter, vtxNodeMap);
            shared++;
        } else {
            free++;
        }
    }
    spdlog::info("  {} shared, {} free", shared, free);

    gmsh::option::setNumber("Mesh.MeshSizeMin", meshSize * 0.5);
    gmsh::option::setNumber("Mesh.MeshSizeMax", meshSize);
    gmsh::option::setNumber("Mesh.MeshOnlyEmpty", 1);
    gmsh::option::setNumber("Mesh.SaveAll", 1);
    gmsh::option::setNumber("Mesh.Algorithm", 6);
    gmsh::option::setNumber("Mesh.RecombineAll", 0);

    try {
        if (!configureSurfaceMeshType(faceTag, meshType, gmshToOcc, state, meshSize)) {
            spdlog::warn("  Cannot configure {} mesh", surfaceMeshTypeName(meshType));
            storeNewEdges(state, gmshToOcc);
            gmsh::finalize();
            return result;
        }
        gmsh::model::mesh::generate(2);
    } catch (const std::exception& e) {
        spdlog::error("  Mesh failed: {}", e.what());
        storeNewEdges(state, gmshToOcc);
        gmsh::finalize();
        return result;
    }

    // 检查面单元
    {
        std::vector<int> ct;
        std::vector<std::vector<std::size_t>> cta, cno;
        gmsh::model::mesh::getElements(ct, cta, cno, 2, faceTag);
        bool has = false;
        for (size_t t = 0; t < ct.size(); ++t)
            if ((ct[t] == 2 || ct[t] == 3) && !cta[t].empty()) {
                has = true;
                break;
            }
        if (!has) {
            spdlog::warn("  No surface elements");
            storeNewEdges(state, gmshToOcc);
            gmsh::finalize();
            return result;
        }
    }

    result = extractFaceMesh(faceTag);
    storeNewEdges(state, gmshToOcc);
    state.meshedFacesCache[faceIndex] = result; // 缓存面结果

    if (result.success) {
        mergeMeshResult(mesh_data, state, model_layer, result);
    }
    gmsh::finalize();
    return result;
}

double IncrementalMeshTools::estimateMeshSize(const GeometryData& geometry)
{
    if (!geometry.rootShape || geometry.rootShape->IsNull())
        return 10.0;
    GProp_GProps props;
    BRepGProp::SurfaceProperties(*geometry.rootShape, props);
    double area = props.Mass();
    if (area > 0) {
        double s = std::sqrt(area) / 10.0;
        if (s < 0.01)
            s = 0.01;
        if (s > 100.0)
            s = 100.0;
        return s;
    }
    return 10.0;
}

std::size_t IncrementalMeshTools::faceCount(const GmshIncrementalMeshState& state)
{
    return state.meshContext ? state.meshContext->faceCount() : 0;
}

std::size_t IncrementalMeshTools::meshedEdgeCount(const GmshIncrementalMeshState& state)
{
    return state.meshedEdgeRefCounts.size();
}

bool IncrementalMeshTools::writeSingleFaceObj(const SingleFaceMeshResult& res, const std::filesystem::path& filepath)
{
    if (!res.success)
        return false;

    std::ofstream ofs(filepath);
    if (!ofs.is_open())
        return false;

    for (const auto& v : res.vertices)
        ofs << "v " << v[0] << " " << v[1] << " " << v[2] << "\n";

    for (size_t i = 0; i + 1 < res.face_vertices_offset.size(); ++i) {
        size_t start = res.face_vertices_offset[i];
        size_t end = res.face_vertices_offset[i + 1];
        ofs << "f";
        for (size_t j = start; j < end; ++j)
            ofs << " " << (res.face_vertices[j] + 1);
        ofs << "\n";
    }
    return true;
}

bool IncrementalMeshTools::writeMeshObj(
    const MeshData& res,
    const GmshIncrementalMeshState& state,
    const std::filesystem::path& filepath)
{
    std::ofstream ofs(filepath);
    if (!ofs.is_open())
        return false;

    ofs << "o GmshMergedMesh\n";
    ofs << "g 0\n"; 

    for (const auto& v : res.vertex_positions_)
        ofs << "v " << v[0] << " " << v[1] << " " << v[2] << "\n";

    std::unordered_map<Index, std::size_t> globalToLocal;
    const auto& globalIds = state.local_to_global_point_ids;
    for (std::size_t i = 0; i < globalIds.size(); ++i) {
        globalToLocal[globalIds[i]] = i + 1;
    }

    for (size_t i = 0; i + 1 < res.face_vertices_offset_.size(); ++i) {
        size_t start = res.face_vertices_offset_[i];
        size_t end = res.face_vertices_offset_[i + 1];
        ofs << "f";
        for (size_t j = start; j < end; ++j) {
            auto it = globalToLocal.find(res.face_vertices_[j]);
            if (it == globalToLocal.end()) {
                spdlog::error("GmshMesh: missing local point for global id {}", res.face_vertices_[j]);
                return false;
            }
            ofs << " " << it->second;
        }
        ofs << "\n";
    }
    return true;
}

bool IncrementalMeshTools::deleteFaceMesh(
    MeshData& mesh_data,
    GmshIncrementalMeshState& state,
    ModelLayer& model_layer,
    std::size_t faceIndex)
{
    // 检查是否存在该面的缓存
    auto it = state.meshedFacesCache.find(faceIndex);
    if (it == state.meshedFacesCache.end()) {
        spdlog::warn("Face {} is not meshed or not cached.", faceIndex);
        return false;
    }

    // 释放面的边界边（更新引用计数并在归零时移除）
    if (state.meshContext) {
        TopoDS_Face face = state.meshContext->getFaceByIndex(faceIndex);
        auto occEdges = state.meshContext->getFaceEdgeIds(face);
        for (GeomEdgeId globalEdgeId : occEdges) {
            auto refIt = state.meshedEdgeRefCounts.find(globalEdgeId);
            if (refIt != state.meshedEdgeRefCounts.end()) {
                refIt->second--;
                if (refIt->second <= 0) {
                    state.meshedEdgeRefCounts.erase(refIt);
                    state.meshedEdgesCache.erase(globalEdgeId);
                    spdlog::info("Edge {} cache cleaned up.", globalEdgeId);
                }
            }
        }
    }

    state.meshedFacesCache.erase(it);
    
    // 重构meshdata 暂时重建面索引
    mesh_data.face_vertices_.clear();
    mesh_data.face_vertices_offset_.clear();
    mesh_data.face_vertices_offset_.push_back(0); 

    TempNodeLookup lookup(mesh_data, state, model_layer, 1e-7);

    // 遍历目前还保留的所有有效面，按序装入MeshData
    for (const auto& [fIdx, faceMeshResult] : state.meshedFacesCache) {
        if (!faceMeshResult.success)
            continue;

        std::vector<Index> localToGlobal(faceMeshResult.vertices.size());
        for (size_t i = 0; i < faceMeshResult.vertices.size(); ++i) {
            localToGlobal[i] = lookup.getOrInsert(
                faceMeshResult.vertices[i][0],
                faceMeshResult.vertices[i][1],
                faceMeshResult.vertices[i][2]);
        }

        for (size_t i = 0; i + 1 < faceMeshResult.face_vertices_offset.size(); ++i) {
            size_t start = faceMeshResult.face_vertices_offset[i];
            size_t end = faceMeshResult.face_vertices_offset[i + 1];
            for (size_t j = start; j < end; ++j) {
                mesh_data.face_vertices_.push_back(
                    localToGlobal[faceMeshResult.face_vertices[j]]);
            }
            mesh_data.face_vertices_offset_.push_back(
                static_cast<Index>(mesh_data.face_vertices_.size()));
        }
    }

    spdlog::info("Deleted mesh for face {}, rebuilt topology. Remaining cells: {}",
        faceIndex, mesh_data.face_vertices_offset_.size() - 1);
    return true;
}

SingleFaceMeshResult IncrementalMeshTools::remeshSingleFace(
    MeshData& mesh_data,
    GeometryData& geometry,
    GmshIncrementalMeshState& state,
    ModelLayer& model_layer,
    std::size_t faceIndex,
    double meshSize,
    int meshTypeIndex)
{
    if (state.meshedFacesCache.find(faceIndex) != state.meshedFacesCache.end()) {
        spdlog::info("Remeshing: Face {} is already meshed, deleting old mesh first.", faceIndex);
        deleteFaceMesh(mesh_data, state, model_layer, faceIndex);
    }
    return meshSingleFace(
        mesh_data, geometry, state, model_layer, faceIndex, meshSize, meshTypeIndex);
}
