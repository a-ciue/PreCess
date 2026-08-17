#include "IncrementalMeshTools.h"

#include "ComponentData.h"
#include "ComponentOperator.h"
#include "GeometryRegistry.h"
#include "GmshMeshOptions.h"
#include "MeshData.h"
#include "MeshIDMap.h"
#include "ModelLayer.h"

#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <TopExp_Explorer.hxx>

#include <gmsh.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#include <unordered_map>

namespace {

// 合并网格时的顶点去重
class TempNodeLookup {
public:
    explicit TempNodeLookup(
        MeshData& mesh_data,
        ComponentOperator& component_op,
        double tolerance = 1e-7)
        : _component_op(component_op)
        , _tolerance(tolerance)
    {
        // 坐标 -> 组件内局部点索引去重表，只查 MeshData 自持坐标
        const auto& vertices = mesh_data.vertex_positions_;
        for (size_t i = 0; i < vertices.size(); ++i) {
            auto qc = _quantize(vertices[i][0], vertices[i][1], vertices[i][2]);
            _map[qc] = static_cast<Index>(i);
        }
    }

    Index getOrInsert(double x, double y, double z)
    {
        auto qc = _quantize(x, y, z);
        auto it = _map.find(qc);
        if (it != _map.end())
            return it->second;

        // 新点：经 ComponentOperator 原子加点（坐标 + vertex_count_ + gid 伴生表）并标脏
        const Index local_id = _component_op.appendPoint({ x, y, z });
        _map[qc] = local_id;
        return local_id;
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

    ComponentOperator& _component_op;
    double _tolerance;
    std::unordered_map<QuantizedCoord, Index, CoordHash> _map;
};

/**
 * @brief 按网格合并节点时的相同量化规则查找组件内局部点 id
 */
class MeshPointLookup {
public:
    explicit MeshPointLookup(const MeshData& mesh)
    {
        for (Index point_id = 0;
             point_id < static_cast<Index>(mesh.vertex_positions_.size()); ++point_id) {
            const auto& position = mesh.vertex_positions_[static_cast<std::size_t>(point_id)];
            points_[quantize(position)] = point_id;
        }
    }

    std::optional<Index> find(const std::array<double, 3>& position) const
    {
        const auto it = points_.find(quantize(position));
        return it == points_.end() ? std::nullopt : std::optional<Index>(it->second);
    }

private:
    struct QuantizedCoord {
        std::int64_t x;
        std::int64_t y;
        std::int64_t z;

        bool operator==(const QuantizedCoord& other) const
        {
            return x == other.x && y == other.y && z == other.z;
        }
    };

    struct CoordHash {
        std::size_t operator()(const QuantizedCoord& coord) const
        {
            std::size_t hash = 0;
            hash ^= std::hash<std::int64_t>()(coord.x)
                + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            hash ^= std::hash<std::int64_t>()(coord.y)
                + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            hash ^= std::hash<std::int64_t>()(coord.z)
                + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            return hash;
        }
    };

    static QuantizedCoord quantize(const std::array<double, 3>& position)
    {
        constexpr double tolerance = 1e-7;
        return {
            static_cast<std::int64_t>(std::round(position[0] / tolerance)),
            static_cast<std::int64_t>(std::round(position[1] / tolerance)),
            static_cast<std::int64_t>(std::round(position[2] / tolerance))
        };
    }

    std::unordered_map<QuantizedCoord, Index, CoordHash> points_;
};

// 通过全局几何面 ID 从 GeometryRegistry 取得 CAD 面。
TopoDS_Face getFaceById(
    const GeometryRegistry& registry,
    GeomFaceId faceId)
{
    const TopoDS_Shape* shape = registry.getFace(faceId);
    return shape ? TopoDS::Face(*shape) : TopoDS_Face {};
}

// 遍历一个 CAD 面，返回其去重后的全局边 ID。
std::vector<GeomEdgeId> getFaceEdgeIds(
    const TopoDS_Face& face,
    const GeometryData& geometry)
{
    const auto& edgeMap =
        geometry.index.type_maps[GeometrySubshapeIndex::typeIndex(TopAbs_EDGE)];

    std::vector<GeomEdgeId> edgeIds;
    for (TopExp_Explorer explorer(face, TopAbs_EDGE); explorer.More(); explorer.Next()) {
        int localEdgeId = edgeMap.FindIndex(explorer.Current());
        GeomEdgeId globalEdgeId = geometry.index.edgeGlobalId(localEdgeId);
        if (globalEdgeId != kInvalidGeomEdgeId
            && std::find(edgeIds.begin(), edgeIds.end(), globalEdgeId) == edgeIds.end()) {
            edgeIds.push_back(globalEdgeId);
        }
    }
    return edgeIds;
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
    const GeometryData& geometry)
{
    std::map<int, GeomEdgeId> result;
    const auto& edgeMap =
        geometry.index.type_maps[GeometrySubshapeIndex::typeIndex(TopAbs_EDGE)];
    std::vector<GeomEdgeId> importedEdgeIds;

    for (TopExp_Explorer explorer(face, TopAbs_EDGE); explorer.More(); explorer.Next()) {
        TopoDS_Edge edge = TopoDS::Edge(explorer.Current());
        int localEdgeId = edgeMap.FindIndex(edge);
        GeomEdgeId globalEdgeId = geometry.index.edgeGlobalId(localEdgeId);
        if (globalEdgeId == kInvalidGeomEdgeId
            || std::find(importedEdgeIds.begin(), importedEdgeIds.end(), globalEdgeId) != importedEdgeIds.end()) {
            continue;
        }
        importedEdgeIds.push_back(globalEdgeId);

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
            spdlog::warn("  Cannot import OCC edge {}", globalEdgeId);
            continue;
        }

        result[gmshTag] = globalEdgeId;
        spdlog::info("  GMSH edge {} -> OCC edge {}", gmshTag, globalEdgeId);
    }

    spdlog::info("  Imported {}/{} OCC edges",
        result.size(), importedEdgeIds.size());
    return result;
}

// 保存结构化划分中一条边的节点数，以及该数量是否由共享边缓存固定。
struct EdgeTransfiniteInfo {
    int gmshTag {};
    int pointCount {};
    bool fixedByExistingMesh {};
};

// 管理一次单面划分使用的 Gmsh 全局会话，确保异常和提前返回时均会释放资源。
class GmshSession {
public:
    GmshSession()
    {
        if (gmsh::isInitialized())
            throw std::runtime_error("GmshMesh: Gmsh session is already initialized");

        try {
            gmsh::initialize();
            ownsSession_ = true;
        } catch (...) {
            if (gmsh::isInitialized())
                gmsh::finalize();
            throw;
        }
    }

    ~GmshSession()
    {
        if (ownsSession_ && gmsh::isInitialized())
            gmsh::finalize();
    }

    GmshSession(const GmshSession&) = delete;
    GmshSession& operator=(const GmshSession&) = delete;

private:
    bool ownsSession_ {};
};

// 设置 Gmsh 数值 option；
void setGmshNumberOption(const std::string& name, double value)
{
    try {
        gmsh::option::setNumber(name, value);
    } catch (const std::exception& e) {
        spdlog::warn("GmshMesh: cannot set option {} = {}: {}", name, value, e.what());
    }
}

// 把用户参数转换为 Gmsh 全局网格选项。只在这里直接写 Gmsh option，便于后续维护默认值。
void configureMeshingOptions(const IncrementalMeshTools::GmshMeshParameters& parameters, double meshSize)
{
    double minSize = parameters.minMeshSize > 0.0 ? parameters.minMeshSize : meshSize * 0.5;
    double maxSize = parameters.maxMeshSize > 0.0 ? parameters.maxMeshSize : meshSize;

    setGmshNumberOption("Mesh.MeshSizeMin", minSize);
    setGmshNumberOption("Mesh.MeshSizeMax", maxSize);
    setGmshNumberOption("Mesh.MeshOnlyEmpty", 1);
    setGmshNumberOption("Mesh.SaveAll", 1);
    setGmshNumberOption("Mesh.Algorithm", parameters.meshAlgorithm);
    setGmshNumberOption("Mesh.RecombineAll", 0);
    setGmshNumberOption("Mesh.Smoothing", parameters.smoothingSteps);

    setGmshNumberOption("Mesh.AlgorithmSwitchOnFailure", parameters.algorithmSwitchOnFailure);
    setGmshNumberOption("Mesh.RecombinationAlgorithm", parameters.recombineAlgorithm);
    if (parameters.quadMinQuality > 0.0)
        setGmshNumberOption("Mesh.RecombineMinimumQuality", parameters.quadMinQuality);
}

// 根据曲线长度和目标网格尺寸估算结构化曲线节点数；或者指定的划分段数
int estimateEdgePointCount(int gmshTag, double meshSize, int structuredEdgeDivisions)
{
    if (structuredEdgeDivisions > 0)
        return std::max(2, structuredEdgeDivisions + 1);

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
    double meshSize,
    int structuredEdgeDivisions)
{
    EdgeTransfiniteInfo info;
    info.gmshTag = gmshTag;

    auto occIt = gmshToOcc.find(gmshTag);
    if (occIt != gmshToOcc.end()) {
        auto cacheIt = state.meshedEdgesCache.find(occIt->second);
        if (cacheIt != state.meshedEdgesCache.end() && !cacheIt->second.coords.empty()) {
            info.pointCount = static_cast<int>(cacheIt->second.coords.size() / 3);
            info.fixedByExistingMesh = true;
            return info;
        }
    }

    info.pointCount = estimateEdgePointCount(gmshTag, meshSize, structuredEdgeDivisions);
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
    double meshSize,
    const IncrementalMeshTools::GmshMeshParameters& parameters)
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
        edges[i] = makeEdgeTransfiniteInfo(
            edgeTags[i], gmshToOcc, state, meshSize, parameters.structuredEdgeDivisions);

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
    if (nodes.coords.empty())
        return false;
    std::size_t nc = nodes.coords.size() / 3;

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
        // 方向改变后原参数顺序不再可用，统一在内部节点收集完成后批量重新参数化。
        orderedParams.clear();
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
        // 从通用 Geometry↔Mesh 映射恢复时没有 Gmsh 私有参数坐标，
        // 由当前导入的 OCC 曲线一次性重新计算，避免逐点投影产生重复参数。
        try {
            gmsh::model::getParametrization(1, gmshTag, innerCoords, innerParams);
        } catch (...) {
            innerParams.clear();
        }

        // 个别曲线无法批量参数化时逐点回退，并保证每个内部节点都有参数值。
        if (innerParams.size() != innerTags.size()) {
            innerParams.clear();
            for (std::size_t i = 0; i < innerTags.size(); ++i) {
                const std::size_t ci = i + 1;
                double parameter = double(i + 1) / double(nc - 1);
                try {
                    std::vector<double> closest_coords;
                    std::vector<double> closest_params;
                    gmsh::model::getClosestPoint(1, gmshTag,
                        { orderedCoords[ci * 3], orderedCoords[ci * 3 + 1], orderedCoords[ci * 3 + 2] },
                        closest_coords, closest_params);
                    if (!closest_params.empty())
                        parameter = closest_params[0];
                } catch (...) {
                }
                innerParams.push_back(parameter);
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
        if (state.meshedEdgesCache.find(oid) == state.meshedEdgesCache.end()) {
            auto ed = extractEdgeNodes(gt);
            if (!ed.coords.empty()) {
                state.meshedEdgesCache[oid] = std::move(ed);
                state.meshedEdgeRefCounts[oid] = 1;
                nNew++;
            }
        } else {
            ++state.meshedEdgeRefCounts[oid];
            nShared++;
        }
    }
    spdlog::info("GmshMesh:  {} new edges stored, {} edges reused (total cached: {})",
        nNew, nShared, state.meshedEdgesCache.size());
}

void mergeMeshResult(
    ComponentOperator& component_op,
    SingleFaceMeshResult& result)
{
    if (!result.success)
        return;

    // 经可写入口获取网格：获取即标脏（Topology 失效邻接懒表 + 记入待通知集合）
    MeshData& mesh_data = component_op.editableMesh();
    if (mesh_data.face_vertices_offset_.empty())
        mesh_data.face_vertices_offset_.push_back(0);

    TempNodeLookup lookup(mesh_data, component_op, 1e-7);

    // Gmsh 结果顶点 -> 组件内局部点 id（同坐标点去重复用）
    std::vector<Index> resultToLocal(result.vertices.size());
    for (size_t i = 0; i < result.vertices.size(); ++i) {
        resultToLocal[i] = lookup.getOrInsert(
            result.vertices[i][0],
            result.vertices[i][1],
            result.vertices[i][2]);
    }

    result.global_face_vertices.clear();
    result.global_face_vertices.reserve(result.face_vertices.size());

    for (size_t i = 0; i + 1 < result.face_vertices_offset.size(); ++i) {
        size_t start = result.face_vertices_offset[i];
        size_t end = result.face_vertices_offset[i + 1];
        for (size_t j = start; j < end; ++j) {
            Index localPointId = resultToLocal[result.face_vertices[j]];
            result.global_face_vertices.push_back(localPointId);
            mesh_data.face_vertices_.push_back(localPointId);
        }
        mesh_data.face_vertices_offset_.push_back(
            static_cast<Index>(mesh_data.face_vertices_.size()));
    }

    spdlog::info("GmshMesh: merged total {} nodes, {} cells",
        mesh_data.vertex_positions_.size(),
        mesh_data.face_vertices_offset_.size() - 1);
}

// 从 MeshData 中移除指定几何面映射的单元，保留其他算法已写入的单元。
bool removeMappedFaceCells(
    ComponentOperator& component_op,
    const GeometryFaceMeshTopology& topology)
{
    const MeshData* mesh = component_op.mesh();
    if (!mesh) {
        spdlog::error("GmshMesh: component has no mesh");
        return false;
    }
    if (topology.face_vertices_offset.size() < 2) {
        spdlog::error("GmshMesh: mapped face topology is incomplete");
        return false;
    }

    std::set<std::vector<Index>> targetCells;
    for (std::size_t i = 0; i + 1 < topology.face_vertices_offset.size(); ++i) {
        const Index begin = topology.face_vertices_offset[i];
        const Index end = topology.face_vertices_offset[i + 1];
        if (begin < 0 || begin > end
            || end > static_cast<Index>(topology.face_vertices.size())) {
            return false;
        }
        targetCells.emplace(topology.face_vertices.begin() + begin,
            topology.face_vertices.begin() + end);
    }

    std::vector<Index> keptVertices;
    std::vector<Index> keptOffsets { 0 };
    for (std::size_t i = 0; i + 1 < mesh->face_vertices_offset_.size(); ++i) {
        const Index begin = mesh->face_vertices_offset_[i];
        const Index end = mesh->face_vertices_offset_[i + 1];
        if (begin < 0 || end < begin
            || end > static_cast<Index>(mesh->face_vertices_.size())) {
            spdlog::error("GmshMesh: invalid MeshData face offsets");
            return false;
        }

        std::vector<Index> cell(mesh->face_vertices_.begin() + begin,
            mesh->face_vertices_.begin() + end);
        if (targetCells.erase(cell) == 0) {
            keptVertices.insert(keptVertices.end(), cell.begin(), cell.end());
            keptOffsets.push_back(static_cast<Index>(keptVertices.size()));
        }
    }

    if (!targetCells.empty()) {
        spdlog::error("GmshMesh: cached face cells are missing from MeshData");
        return false;
    }

    // 校验通过，写回剩余单元（经可写入口裸写：获取即标脏）
    MeshData& mesh_data = component_op.editableMesh();
    mesh_data.face_vertices_ = std::move(keptVertices);
    mesh_data.face_vertices_offset_ = std::move(keptOffsets);
    return true;
}

// 复制本次划分会修改的网格数组，用于重划分的候选结果。
// 提交候选划分产生的网格数组，不影响 MeshData 的属性、边和体数据。
} // anonymous namespace

// 公开接口
std::optional<GmshIncrementalMeshState> IncrementalMeshTools::buildStateFromGeometryMeshMap(
    const GeometryMeshMap* mapping,
    const MeshData& mesh,
    const GeometryData& geometry,
    const GeometryRegistry& registry)
{
    GmshIncrementalMeshState state;
    if (!mapping)
        return state;

    for (const auto& [edge_id, point_ids] : mapping->geometry_edge_to_mesh_point_ids) {
        MeshedEdgeData edge_data;
        edge_data.coords.reserve(point_ids.size() * 3);
        for (Index point_id : point_ids) {
            if (point_id < 0
                || point_id >= static_cast<Index>(mesh.vertex_positions_.size())) {
                spdlog::error(
                    "GmshMesh: geometry edge {} references invalid mesh point {}",
                    edge_id, point_id);
                return std::nullopt;
            }
            const auto& position = mesh.vertex_positions_[static_cast<std::size_t>(point_id)];
            edge_data.coords.insert(
                edge_data.coords.end(), position.begin(), position.end());
        }
        state.meshedEdgesCache.emplace(edge_id, std::move(edge_data));
    }

    // 引用计数只作为本次操作的派生缓存，由已映射面与 CAD 边关系一次性重建。
    for (const auto& [face_id, unused] : mapping->geometry_face_to_mesh_topology) {
        (void)unused;
        const TopoDS_Shape* shape = registry.getFace(face_id);
        if (!shape)
            continue;
        for (GeomEdgeId edge_id : getFaceEdgeIds(TopoDS::Face(*shape), geometry)) {
            if (state.meshedEdgesCache.find(edge_id) != state.meshedEdgesCache.end())
                ++state.meshedEdgeRefCounts[edge_id];
        }
    }

    return state;
}

bool IncrementalMeshTools::storeStateToGeometryMeshMap(
    const GmshIncrementalMeshState& state,
    const GeometryMeshMap& working_mapping,
    ComponentOperator& component_op)
{
    const MeshData* mesh = component_op.mesh();
    if (!mesh)
        return false;

    GeometryMeshMap candidate = working_mapping;
    candidate.geometry_edge_to_mesh_point_ids.clear();
    const MeshPointLookup point_lookup(*mesh);
    for (const auto& [edge_id, edge_data] : state.meshedEdgesCache) {
        if (edge_data.coords.size() % 3 != 0) {
            spdlog::error("GmshMesh: geometry edge {} has invalid coordinate cache", edge_id);
            return false;
        }

        auto& point_ids = candidate.geometry_edge_to_mesh_point_ids[edge_id];
        point_ids.reserve(edge_data.coords.size() / 3);
        for (std::size_t i = 0; i < edge_data.coords.size(); i += 3) {
            const std::array<double, 3> position {
                edge_data.coords[i], edge_data.coords[i + 1], edge_data.coords[i + 2]
            };
            const auto point_id = point_lookup.find(position);
            if (!point_id) {
                spdlog::error(
                    "GmshMesh: geometry edge {} node is missing from MeshData", edge_id);
                return false;
            }
            point_ids.push_back(*point_id);
        }
    }

    component_op.editableGeometryMeshMap() = std::move(candidate);
    return true;
}

SingleFaceMeshResult IncrementalMeshTools::meshSingleFace(
    GeometryData& geometry,
    GmshIncrementalMeshState& state,
    GeometryMeshMap& working_mapping,
    ComponentOperator& component_op,
    GeomFaceId faceId,
    double meshSize,
    const GmshMeshParameters& parameters)
{
    SingleFaceMeshResult result;
    GmshSurfaceMeshType meshType = parseSurfaceMeshType(parameters.meshTypeIndex);

    spdlog::info("=== Meshing face {} (size={:.6f}, type={}) ===",
        faceId, meshSize,
        surfaceMeshTypeName(meshType));

    TopoDS_Face face = getFaceById(component_op.manager().geomRegistry(), faceId);
    if (face.IsNull()) {
        spdlog::error("Face {} is null or invalid", faceId);
        return result;
    }
    GmshSession session;
    setGmshNumberOption("General.Terminal", 1);
    gmsh::model::add("face_model");
    // 先逐边导入并记录返回 tag，再导入整个面；面导入会复用相同 OCC 边的 tag。
    auto gmshToOcc = importGmshEdges(face, geometry);

    gmsh::vectorpair outDimTags;
    gmsh::model::occ::importShapesNativePointer(
        static_cast<const void*>(&face), outDimTags);
    gmsh::model::occ::synchronize();

    std::vector<std::pair<int, int>> faceDimTags;
    gmsh::model::getEntities(faceDimTags, 2);
    if (faceDimTags.empty()) {
        spdlog::error("  No face after import");
        return result;
    }
    int faceTag = faceDimTags[0].second;

    std::size_t nodeCounter = 1, elemCounter = 1;
    int shared = 0, free = 0;
    std::map<int, std::size_t> vtxNodeMap;

    for (const auto& [gt, oid] : gmshToOcc) {
        auto cacheIt = state.meshedEdgesCache.find(oid);
        if (cacheIt != state.meshedEdgesCache.end()) {
            if (!injectConstrainedEdge(gt, cacheIt->second,
                    nodeCounter, elemCounter, vtxNodeMap)) {
                spdlog::error("GmshMesh: failed to inject constrained edge {}", oid);
                return result;
            }
            shared++;
        } else {
            free++;
        }
    }
    spdlog::info("  {} shared, {} free", shared, free);

    configureMeshingOptions(parameters, meshSize);

    try {
        if (!configureSurfaceMeshType(faceTag, meshType, gmshToOcc, state, meshSize, parameters)) {
            spdlog::warn("  Cannot configure {} mesh", surfaceMeshTypeName(meshType));
            return result;
        }
        gmsh::model::mesh::generate(2);
    } catch (const std::exception& e) {
        spdlog::error("  Mesh failed: {}", e.what());
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
            return result;
        }
    }

    result = extractFaceMesh(faceTag);
    if (result.success) {
        storeNewEdges(state, gmshToOcc);
        mergeMeshResult(component_op, result);

        GeometryFaceMeshTopology topology;
        topology.face_vertices = result.global_face_vertices;
        topology.face_vertices_offset.reserve(result.face_vertices_offset.size());
        for (std::size_t offset : result.face_vertices_offset)
            topology.face_vertices_offset.push_back(static_cast<Index>(offset));
        working_mapping.geometry_face_to_mesh_topology[faceId] = std::move(topology);
    }
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

std::size_t IncrementalMeshTools::faceCount(const GeometryData& geometry)
{
    if (!geometry.index.built)
        return 0;
    return static_cast<std::size_t>(
        geometry.index.type_maps[GeometrySubshapeIndex::typeIndex(TopAbs_FACE)].Extent());
}


bool IncrementalMeshTools::deleteFaceMesh(
    GeometryData& geometry,
    GmshIncrementalMeshState& state,
    GeometryMeshMap& working_mapping,
    ComponentOperator& component_op,
    GeomFaceId faceId)
{
    auto face_it = working_mapping.geometry_face_to_mesh_topology.find(faceId);
    if (face_it == working_mapping.geometry_face_to_mesh_topology.end()) {
        spdlog::warn("Face {} is not present in geometry-mesh mapping.", faceId);
        return false;
    }

    if (!removeMappedFaceCells(component_op, face_it->second))
        return false;

    // 面删除后递减临时引用计数，仅清理不再被其他已映射面使用的边缓存。
    const TopoDS_Face face = getFaceById(component_op.manager().geomRegistry(), faceId);
    for (GeomEdgeId edge_id : getFaceEdgeIds(face, geometry)) {
        auto ref_it = state.meshedEdgeRefCounts.find(edge_id);
        if (ref_it == state.meshedEdgeRefCounts.end())
            continue;
        if (--ref_it->second <= 0) {
            state.meshedEdgeRefCounts.erase(ref_it);
            state.meshedEdgesCache.erase(edge_id);
        }
    }
    working_mapping.geometry_face_to_mesh_topology.erase(face_it);

    const MeshData* mesh = component_op.mesh();
    spdlog::info("Deleted Gmsh mesh for face {}. Remaining cells: {}",
        faceId, mesh ? mesh->face_vertices_offset_.size() - 1 : 0);
    return true;
}

SingleFaceMeshResult IncrementalMeshTools::remeshSingleFace(
    GeometryData& geometry,
    GmshIncrementalMeshState& state,
    GeometryMeshMap& working_mapping,
    ComponentOperator& component_op,
    GeomFaceId faceId,
    double meshSize,
    const GmshMeshParameters& parameters)
{
    if (working_mapping.geometry_face_to_mesh_topology.find(faceId)
        != working_mapping.geometry_face_to_mesh_topology.end()) {
        deleteFaceMesh(geometry, state, working_mapping, component_op, faceId);
    }

    return meshSingleFace(
        geometry, state, working_mapping, component_op, faceId, meshSize, parameters);
}
