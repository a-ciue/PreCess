/**
 * @file DimensionHandler.cpp
 * @brief 尺寸标注处理器：网格与几何双路径的距离、角度、半径、长度、面积、体积、包围盒与重心
 * @author 范成通 email 1941804585@qq.com
 */

#include "DimensionHandler.h"
#include "ArgObject.h"
#include "ComponentData.h"
#include "FeatureContext.h"
#include "FeatureParams.h"
#include "FeatureRegistrar.h"
#include "GeometryData.h"
#include "GeometryRegistry.h"
#include "MeshAdjacency.h"
#include "MeshData.h"
#include "MeshIDMap.h"
#include "ModelLayer.h"
#include "Selection.h"

#include <Bnd_Box.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <BRep_Tool.hxx>
#include <GeomAbs_CurveType.hxx>
#include <GProp_GProps.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Vertex.hxx>
#include <gp_Circ.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numeric>
#include <sstream>
#include <string>

namespace systems::feature {

namespace {
using Vec3 = std::array<double, 3>;

// 参数下标：测量类型 / 选择对象（与 setup() 注册顺序一致）
constexpr std::size_t kTypeParam = 0;
constexpr std::size_t kSelectionParam = 1;

Vec3 operator-(const Vec3& a, const Vec3& b)
{
    return { a[0] - b[0], a[1] - b[1], a[2] - b[2] };
}

Vec3 operator+(const Vec3& a, const Vec3& b)
{
    return { a[0] + b[0], a[1] + b[1], a[2] + b[2] };
}

Vec3 operator*(const Vec3& a, double s)
{
    return { a[0] * s, a[1] * s, a[2] * s };
}

Vec3 operator/(const Vec3& a, double s)
{
    return { a[0] / s, a[1] / s, a[2] / s };
}

//! @brief 两点中点
Vec3 midpoint(const Vec3& a, const Vec3& b)
{
    return { (a[0] + b[0]) / 2.0, (a[1] + b[1]) / 2.0, (a[2] + b[2]) / 2.0 };
}

double dot(const Vec3& a, const Vec3& b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

double length(const Vec3& v)
{
    return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

Vec3 cross(const Vec3& a, const Vec3& b)
{
    return {
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0]
    };
}

enum class MeasureType {
    Distance,
    Angle,
    Radius,
    Length,
    Area,
    Volume,
    BoundingBox,
    Centroid
};

//! @brief 网格测量实现签名
using MeshExecFn = std::string (*)(const MeshData&, ModelLayer&, const Selection&);
//! @brief 几何测量实现签名
using GeomExecFn = std::string (*)(const GeometryRegistry&, const Selection&);

//! @brief 测量操作表项：名称、选择器模式与两套实现的统一注册处（新增类型只需在 kMeasureOps 加一行）
struct MeasureOp {
    MeasureType type;
    const char* name; //> Combo 显示名（表内顺序即下标）
    const char* mesh_selector_modes; //> 网格组件的选择器模式
    const char* geom_selector_modes; //> 几何组件的选择器模式
    MeshExecFn mesh_exec; //> 网格实现
    GeomExecFn geom_exec; //> 几何实现（nullptr 表示暂不支持几何模型）
};

/**
 * @brief 取选择集中全局点 id 对应的顶点坐标
 *
 * 选择集携带的顶点 id 为全局点 id（core/Selection.h 约定），经 pointIdMap 换算到所属
 * 组件的局部点后取坐标，跨组件选择因此天然支持；连通性数组内的局部点索引直接索引
 * mesh.vertex_positions_，不走此接口。
 *
 * @return 坐标指针；id 无映射、组件无网格或局部点越界时返回 nullptr
 */
const Vec3* getPosition(const ModelLayer& manager, Index id)
{
    const auto [component_id, local] = manager.pointIdMap().getLocal(id);
    if (component_id == MeshIDMap::kInvalidComponent)
        return nullptr;
    const ComponentData* comp = manager.findComponent(component_id);
    if (!comp || !comp->mesh)
        return nullptr;
    if (local < 0 || local >= comp->mesh->vertex_count_)
        return nullptr;
    return &comp->mesh->vertex_positions_[static_cast<std::size_t>(local)];
}

/**
 * @brief 稳定局部边 id 解析为两端点坐标
 *
 * 边选择携带的 id 为稳定局部边 id（core/Selection.h 契约），经所属组件的
 * MeshAdjacency 解析出端点（组件内局部点 id）后直接索引 mesh.vertex_positions_。
 *
 * @return 解析成功返回 true；边 id 无效（消亡/越界）或端点越界返回 false
 */
bool edgeEndpointPositions(ComponentData& comp, const MeshData& mesh, Index stable_edge_id, Vec3& p0, Vec3& p1)
{
    const auto endpoints = comp.mesh_adjacency.edgeEndpoints(mesh, stable_edge_id);
    if (!endpoints)
        return false;

    const Index v0 = (*endpoints)[0];
    const Index v1 = (*endpoints)[1];
    if (v0 < 0 || v0 >= mesh.vertex_count_ || v1 < 0 || v1 >= mesh.vertex_count_)
        return false;

    p0 = mesh.vertex_positions_[static_cast<std::size_t>(v0)];
    p1 = mesh.vertex_positions_[static_cast<std::size_t>(v1)];
    return true;
}

std::string toString(double value, int precision = 6)
{
    std::ostringstream oss;
    oss.setf(std::ios::fixed, std::ios::floatfield);
    oss.precision(precision);
    oss << value;
    return oss.str();
}

std::string vecString(const Vec3& v)
{
    return "(" + toString(v[0]) + ", " + toString(v[1]) + ", " + toString(v[2]) + ")";
}

double angleBetween(const Vec3& u, const Vec3& v)
{
    const double lu = length(u);
    const double lv = length(v);
    if (lu < std::numeric_limits<double>::epsilon() || lv < std::numeric_limits<double>::epsilon())
        return 0.0;

    double cos_theta = dot(u, v) / (lu * lv);
    cos_theta = std::max(-1.0, std::min(1.0, cos_theta));
    return std::acos(cos_theta) * 180.0 / 3.14159265358979323846;
}

double triangleArea(const Vec3& a, const Vec3& b, const Vec3& c)
{
    return 0.5 * length(cross(b - a, c - a));
}

double polygonArea(const std::vector<Vec3>& points)
{
    const size_t n = points.size();
    if (n < 3)
        return 0.0;

    double area = 0.0;
    for (size_t i = 2; i < n; ++i)
        area += triangleArea(points[0], points[i - 1], points[i]);
    return area;
}

double tetraVolume(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d)
{
    return std::abs(dot(b - a, cross(c - a, d - a))) / 6.0;
}

double polyhedronVolume(const MeshData& mesh, Index solid_id)
{
    if (solid_id + 1 >= static_cast<Index>(mesh.solid_faces_offset_.size()))
        return 0.0;

    const Index face_start = mesh.solid_faces_offset_[solid_id];
    const Index face_end = mesh.solid_faces_offset_[solid_id + 1];
    double volume = 0.0;

    for (Index f = face_start; f < face_end; ++f) {
        const Index face_id = mesh.solid_faces_[f];
        if (face_id + 1 >= static_cast<Index>(mesh.solid_faces_vertices_offset_.size()))
            continue;

        const Index vert_start = mesh.solid_faces_vertices_offset_[face_id];
        const Index vert_end = mesh.solid_faces_vertices_offset_[face_id + 1];
        if (vert_end - vert_start < 3)
            continue;

        const Vec3& v0 = mesh.vertex_positions_[static_cast<std::size_t>(mesh.solid_faces_vertices_[vert_start])];
        for (Index i = vert_start + 1; i + 1 < vert_end; ++i) {
            const Vec3& vi = mesh.vertex_positions_[static_cast<std::size_t>(mesh.solid_faces_vertices_[i])];
            const Vec3& vj = mesh.vertex_positions_[static_cast<std::size_t>(mesh.solid_faces_vertices_[i + 1])];
            volume += dot(v0, cross(vi, vj)) / 6.0;
        }
    }

    return std::abs(volume);
}

double solidVolume(const MeshData& mesh, Index solid_id)
{
    if (solid_id < 0 || solid_id + 1 >= static_cast<Index>(mesh.solid_vertices_offset_.size()))
        return 0.0;
    if (solid_id >= static_cast<Index>(mesh.solid_types_.size()))
        return 0.0;

    const unsigned char cell_type = mesh.solid_types_[solid_id];
    const Index start = mesh.solid_vertices_offset_[solid_id];
    const Index end = mesh.solid_vertices_offset_[solid_id + 1];

    // VTK_TETRA = 10
    if (cell_type == 10) {
        if (end - start != 4)
            return 0.0;
        const Vec3& a = mesh.vertex_positions_[static_cast<std::size_t>(mesh.solid_vertices_[start])];
        const Vec3& b = mesh.vertex_positions_[static_cast<std::size_t>(mesh.solid_vertices_[start + 1])];
        const Vec3& c = mesh.vertex_positions_[static_cast<std::size_t>(mesh.solid_vertices_[start + 2])];
        const Vec3& d = mesh.vertex_positions_[static_cast<std::size_t>(mesh.solid_vertices_[start + 3])];
        return tetraVolume(a, b, c, d);
    }

    // VTK_POLYHEDRON = 42
    if (cell_type == 42)
        return polyhedronVolume(mesh, solid_id);

    return 0.0;
}

std::vector<Vec3> collectPositions(const MeshData& mesh, ModelLayer& manager, const Selection& selection)
{
    std::vector<Vec3> positions;
    positions.reserve(selection.ids.size() * 4);

    switch (selection.type) {
    case ElementEnum::Vertex:
        // 选择集顶点 id 为全局点 id，经 pointIdMap 换算取坐标
        for (Index id : selection.ids) {
            if (const Vec3* p = getPosition(manager, id))
                positions.push_back(*p);
        }
        break;
    case ElementEnum::Edge: {
        // 边 id 为稳定局部边 id，经所属组件邻接表解析端点坐标
        ComponentData* comp = manager.findComponent(selection.component_id);
        if (comp) {
            for (Index id : selection.ids) {
                Vec3 p0, p1;
                if (edgeEndpointPositions(*comp, mesh, id, p0, p1)) {
                    positions.push_back(p0);
                    positions.push_back(p1);
                }
            }
        }
        break;
    }
    case ElementEnum::Face:
        for (Index id : selection.ids) {
            if (id + 1 >= static_cast<Index>(mesh.face_vertices_offset_.size()))
                continue;
            for (Index i = mesh.face_vertices_offset_[id]; i < mesh.face_vertices_offset_[id + 1]; ++i) {
                const Index v = mesh.face_vertices_[i];
                if (v >= 0 && v < mesh.vertex_count_)
                    positions.push_back(mesh.vertex_positions_[static_cast<std::size_t>(v)]);
            }
        }
        break;
    case ElementEnum::Solid:
        for (Index id : selection.ids) {
            if (id + 1 >= static_cast<Index>(mesh.solid_vertices_offset_.size()))
                continue;
            for (Index i = mesh.solid_vertices_offset_[id]; i < mesh.solid_vertices_offset_[id + 1]; ++i) {
                const Index v = mesh.solid_vertices_[i];
                if (v >= 0 && v < mesh.vertex_count_)
                    positions.push_back(mesh.vertex_positions_[static_cast<std::size_t>(v)]);
            }
        }
        break;
    default:
        break;
    }

    return positions;
}

std::string formatDistance(const Vec3& a, const Vec3& b)
{
    const Vec3 d = b - a;
    std::ostringstream oss;
    oss << "Distance: " << toString(length(d)) << "\n"
        << "Dx: " << toString(d[0]) << "\n"
        << "Dy: " << toString(d[1]) << "\n"
        << "Dz: " << toString(d[2]);
    return oss.str();
}

std::string formatAngle(const Vec3& a, const Vec3& b, const Vec3& c)
{
    std::ostringstream oss;
    oss << "Angle: " << toString(angleBetween(a - b, c - b)) << " deg";
    return oss.str();
}

std::string formatRadius(const Vec3& a, const Vec3& b, const Vec3& c)
{
    const Vec3 ab = b - a;
    const Vec3 ac = c - a;
    const Vec3 n = cross(ab, ac);
    const double n_len2 = dot(n, n);

    if (n_len2 < std::numeric_limits<double>::epsilon())
        return "错误：三点共线或重合，无法计算半径";

    const Vec3 circumcenter = a + (cross(ac, n) * dot(ab, ab) - cross(ab, n) * dot(ac, ac)) / (2.0 * n_len2);
    const double radius = length(circumcenter - a);

    std::ostringstream oss;
    oss << "Radius: " << toString(radius) << "\n"
        << "Center: " << vecString(circumcenter);
    return oss.str();
}

std::string formatFaceArea(const MeshData& mesh, const std::vector<Index>& ids)
{
    double total = 0.0;
    std::ostringstream oss;

    for (Index id : ids) {
        if (id + 1 >= static_cast<Index>(mesh.face_vertices_offset_.size())) {
            oss << "面 " << id << ": 无效索引\n";
            continue;
        }

        std::vector<Vec3> points;
        for (Index i = mesh.face_vertices_offset_[id]; i < mesh.face_vertices_offset_[id + 1]; ++i)
            points.push_back(mesh.vertex_positions_[static_cast<std::size_t>(mesh.face_vertices_[i])]);

        const double area = polygonArea(points);
        total += area;
        oss << "面 " << id << ": " << toString(area) << "\n";
    }

    oss << "总面积: " << toString(total);
    return oss.str();
}

std::string formatSolidVolume(const MeshData& mesh, const std::vector<Index>& ids)
{
    double total = 0.0;
    std::ostringstream oss;

    for (Index id : ids) {
        const double volume = solidVolume(mesh, id);
        if (volume <= 0.0) {
            oss << "体 " << id << ": 不支持的体类型或无体积\n";
            continue;
        }
        total += volume;
        oss << "体 " << id << ": " << toString(volume) << "\n";
    }

    oss << "总体积: " << toString(total);
    return oss.str();
}

std::string formatBoundingBox(const std::vector<Vec3>& positions)
{
    // 防御性判空：防止被其他路径以空容器调用时越界访问首元素
    if (positions.empty()) {
        spdlog::error("formatBoundingBox: positions 为空");
        return std::string("错误：没有可用的几何数据");
    }

    Vec3 min = positions[0];
    Vec3 max = positions[0];

    for (const auto& p : positions) {
        for (int i = 0; i < 3; ++i) {
            min[i] = std::min(min[i], p[i]);
            max[i] = std::max(max[i], p[i]);
        }
    }

    const Vec3 size = max - min;

    std::ostringstream oss;
    oss << "包围盒:\n"
        << "min: " << vecString(min) << "\n"
        << "max: " << vecString(max) << "\n"
        << "size: " << vecString(size);
    return oss.str();
}

std::string formatCentroid(const std::vector<Vec3>& positions)
{
    // 防御性判空：防止空容器时除零产生 NaN
    if (positions.empty()) {
        spdlog::error("formatCentroid: positions 为空");
        return std::string("错误：没有可用的几何数据");
    }

    Vec3 sum = { 0.0, 0.0, 0.0 };
    for (const auto& p : positions)
        sum = sum + p;
    const Vec3 centroid = sum / static_cast<double>(positions.size());

    std::ostringstream oss;
    oss << "重心: " << vecString(centroid);
    return oss.str();
}

// ---------------- 几何（OCC）测量：选择 id 为 GeometryRegistry 分配的全局几何 id ----------------

Vec3 toVec3(const gp_Pnt& p)
{
    return { p.X(), p.Y(), p.Z() };
}

//! @brief 按选择类型从几何注册表查找子形状，找不到返回 nullptr
const TopoDS_Shape* findGeometryShape(const GeometryRegistry& reg, ElementEnum::Type type, Index id)
{
    switch (type) {
    case ElementEnum::GeometryVertex:
        return reg.getVertex(id);
    case ElementEnum::GeometryEdge:
        return reg.getEdge(id);
    case ElementEnum::GeometryFace:
        return reg.getFace(id);
    case ElementEnum::GeometrySolid:
        return reg.getSolid(id);
    default:
        return nullptr;
    }
}

//! @brief 收集选择中有效的几何子形状
std::vector<TopoDS_Shape> collectGeometryShapes(const GeometryRegistry& reg,
    ElementEnum::Type type, const std::vector<Index>& ids)
{
    std::vector<TopoDS_Shape> shapes;
    shapes.reserve(ids.size());
    for (Index id : ids) {
        if (const TopoDS_Shape* s = findGeometryShape(reg, type, id))
            shapes.push_back(*s);
    }
    return shapes;
}

//! @brief 取几何点坐标
bool geometryVertexPoint(const GeometryRegistry& reg, Index id, Vec3& out)
{
    const TopoDS_Shape* s = reg.getVertex(id);
    if (!s)
        return false;
    out = toVec3(BRep_Tool::Pnt(TopoDS::Vertex(*s)));
    return true;
}

//! @brief 取几何边中点处的切向作为边方向
bool geometryEdgeDirection(const TopoDS_Edge& edge, Vec3& out)
{
    BRepAdaptor_Curve curve(edge);
    const double u = 0.5 * (curve.FirstParameter() + curve.LastParameter());
    gp_Pnt p;
    gp_Vec v;
    curve.D1(u, p, v);
    if (v.Magnitude() < std::numeric_limits<double>::epsilon())
        return false;
    v.Normalize();
    out = { v.X(), v.Y(), v.Z() };
    return true;
}

std::string formatGeometryEdgeLength(const GeometryRegistry& reg, const std::vector<Index>& ids)
{
    double total = 0.0;
    std::ostringstream oss;

    for (Index id : ids) {
        const TopoDS_Shape* s = reg.getEdge(id);
        if (!s) {
            oss << "边 " << id << ": 无效索引\n";
            continue;
        }
        GProp_GProps props;
        BRepGProp::LinearProperties(*s, props);
        const double len = props.Mass();
        total += len;
        oss << "边 " << id << ": " << toString(len) << "\n";
    }

    oss << "累计长度: " << toString(total);
    return oss.str();
}

std::string formatGeometryFaceArea(const GeometryRegistry& reg, const std::vector<Index>& ids)
{
    double total = 0.0;
    std::ostringstream oss;

    for (Index id : ids) {
        const TopoDS_Shape* s = reg.getFace(id);
        if (!s) {
            oss << "面 " << id << ": 无效索引\n";
            continue;
        }
        GProp_GProps props;
        BRepGProp::SurfaceProperties(*s, props);
        const double area = std::abs(props.Mass());
        total += area;
        oss << "面 " << id << ": " << toString(area) << "\n";
    }

    oss << "总面积: " << toString(total);
    return oss.str();
}

std::string formatGeometrySolidVolume(const GeometryRegistry& reg, const std::vector<Index>& ids)
{
    double total = 0.0;
    std::ostringstream oss;

    for (Index id : ids) {
        const TopoDS_Shape* s = reg.getSolid(id);
        if (!s) {
            oss << "体 " << id << ": 无效索引\n";
            continue;
        }
        GProp_GProps props;
        BRepGProp::VolumeProperties(*s, props);
        const double volume = std::abs(props.Mass());
        total += volume;
        oss << "体 " << id << ": " << toString(volume) << "\n";
    }

    oss << "总体积: " << toString(total);
    return oss.str();
}

std::string formatGeometryBoundingBox(const std::vector<TopoDS_Shape>& shapes)
{
    Bnd_Box box;
    for (const auto& s : shapes)
        BRepBndLib::AddOptimal(s, box, false); // 不依赖三角剖分，直接按解析几何求界

    if (box.IsVoid())
        return std::string("错误：无法计算包围盒");

    Standard_Real xmin = 0.0, ymin = 0.0, zmin = 0.0, xmax = 0.0, ymax = 0.0, zmax = 0.0;
    box.Get(xmin, ymin, zmin, xmax, ymax, zmax);
    const Vec3 min { xmin, ymin, zmin };
    const Vec3 max { xmax, ymax, zmax };
    const Vec3 size = max - min;

    std::ostringstream oss;
    oss << "包围盒:\n"
        << "min: " << vecString(min) << "\n"
        << "max: " << vecString(max) << "\n"
        << "size: " << vecString(size);
    return oss.str();
}

//! @brief 按边长 / 面积 / 体积加权的重心
std::string formatGeometryCentroid(const GeometryRegistry& reg,
    ElementEnum::Type type, const std::vector<Index>& ids)
{
    double total_mass = 0.0;
    Vec3 sum { 0.0, 0.0, 0.0 };

    for (Index id : ids) {
        const TopoDS_Shape* s = findGeometryShape(reg, type, id);
        if (!s)
            continue;

        GProp_GProps props;
        switch (type) {
        case ElementEnum::GeometryEdge:
            BRepGProp::LinearProperties(*s, props);
            break;
        case ElementEnum::GeometryFace:
            BRepGProp::SurfaceProperties(*s, props);
            break;
        case ElementEnum::GeometrySolid:
            BRepGProp::VolumeProperties(*s, props);
            break;
        default:
            continue;
        }

        const double mass = std::abs(props.Mass());
        if (mass < std::numeric_limits<double>::epsilon())
            continue;
        sum = sum + toVec3(props.CentreOfMass()) * mass;
        total_mass += mass;
    }

    if (total_mass < std::numeric_limits<double>::epsilon())
        return std::string("错误：未找到可计算重心的几何对象");

    std::ostringstream oss;
    oss << "重心: " << vecString(sum / total_mass);
    return oss.str();
}

// ---------------- 几何（OCC）测量实现：选择 id 为 GeometryRegistry 分配的全局几何 id ----------------

std::string geomDistance(const GeometryRegistry& reg, const Selection& selection)
{
    if (selection.type != ElementEnum::GeometryVertex || selection.ids.size() != 2) {
        return std::string("错误：距离测量需要选择两个几何点");
    }
    Vec3 a, b;
    if (!geometryVertexPoint(reg, selection.ids[0], a) || !geometryVertexPoint(reg, selection.ids[1], b))
        return std::string("错误：无效的几何点 id");
    return formatDistance(a, b);
}

std::string geomAngle(const GeometryRegistry& reg, const Selection& selection)
{
    if (selection.type == ElementEnum::GeometryVertex && selection.ids.size() == 3) {
        Vec3 a, b, c;
        if (!geometryVertexPoint(reg, selection.ids[0], a)
            || !geometryVertexPoint(reg, selection.ids[1], b)
            || !geometryVertexPoint(reg, selection.ids[2], c))
            return std::string("错误：无效的几何点 id");
        return formatAngle(a, b, c);
    }
    if (selection.type == ElementEnum::GeometryEdge && selection.ids.size() == 2) {
        const TopoDS_Shape* e0 = reg.getEdge(selection.ids[0]);
        const TopoDS_Shape* e1 = reg.getEdge(selection.ids[1]);
        Vec3 u, v;
        if (!e0 || !e1
            || !geometryEdgeDirection(TopoDS::Edge(*e0), u)
            || !geometryEdgeDirection(TopoDS::Edge(*e1), v))
            return std::string("错误：无效的几何边 id 或边方向无法计算");
        std::ostringstream oss;
        oss << "Angle: " << toString(angleBetween(u, v)) << " deg";
        return oss.str();
    }
    return std::string("错误：角度测量需要选择三个几何点或两条几何边");
}

std::string geomRadius(const GeometryRegistry& reg, const Selection& selection)
{
    if (selection.type == ElementEnum::GeometryVertex && selection.ids.size() == 3) {
        Vec3 a, b, c;
        if (!geometryVertexPoint(reg, selection.ids[0], a)
            || !geometryVertexPoint(reg, selection.ids[1], b)
            || !geometryVertexPoint(reg, selection.ids[2], c))
            return std::string("错误：无效的几何点 id");
        return formatRadius(a, b, c);
    }
    if (selection.type == ElementEnum::GeometryEdge && selection.ids.size() == 1) {
        const TopoDS_Shape* s = reg.getEdge(selection.ids[0]);
        if (!s)
            return std::string("错误：无效的几何边 id");
        BRepAdaptor_Curve curve(TopoDS::Edge(*s));
        if (curve.GetType() != GeomAbs_Circle)
            return std::string("错误：所选边不是圆弧，无法直接测半径（可改选三个几何点）");
        const gp_Circ circ = curve.Circle();
        std::ostringstream oss;
        oss << "Radius: " << toString(circ.Radius()) << "\n"
            << "Center: " << vecString(toVec3(circ.Location()));
        return oss.str();
    }
    return std::string("错误：半径测量需要选择三个几何点或一条圆弧边");
}

std::string geomLength(const GeometryRegistry& reg, const Selection& selection)
{
    if (selection.type != ElementEnum::GeometryEdge || selection.ids.empty()) {
        return std::string("错误：长度测量需要选择几何边");
    }
    return formatGeometryEdgeLength(reg, selection.ids);
}

std::string geomArea(const GeometryRegistry& reg, const Selection& selection)
{
    if (selection.type != ElementEnum::GeometryFace || selection.ids.empty()) {
        return std::string("错误：面积测量需要选择几何面");
    }
    return formatGeometryFaceArea(reg, selection.ids);
}

std::string geomVolume(const GeometryRegistry& reg, const Selection& selection)
{
    if (selection.type != ElementEnum::GeometrySolid || selection.ids.empty()) {
        return std::string("错误：体积测量需要选择几何体");
    }
    return formatGeometrySolidVolume(reg, selection.ids);
}

std::string geomBoundingBox(const GeometryRegistry& reg, const Selection& selection)
{
    const auto shapes = collectGeometryShapes(reg, selection.type, selection.ids);
    if (shapes.empty()) {
        return std::string("错误：未找到有效的几何对象");
    }
    return formatGeometryBoundingBox(shapes);
}

std::string geomCentroid(const GeometryRegistry& reg, const Selection& selection)
{
    if (selection.type == ElementEnum::GeometryVertex) {
        std::vector<Vec3> points;
        points.reserve(selection.ids.size());
        for (Index id : selection.ids) {
            Vec3 p;
            if (geometryVertexPoint(reg, id, p))
                points.push_back(p);
        }
        if (points.empty())
            return std::string("错误：未找到有效的几何点");
        return formatCentroid(points);
    }
    if (selection.ids.empty()) {
        return std::string("错误：未找到有效的几何对象");
    }
    return formatGeometryCentroid(reg, selection.type, selection.ids);
}

// ---------------- 网格测量实现 ----------------

std::string meshDistance(const MeshData&, ModelLayer& manager, const Selection& selection)
{
    if (selection.type != ElementEnum::Vertex || selection.ids.size() != 2) {
        spdlog::error("DimensionHandler::execute: 距离测量需要选择两个点");
        return std::string("错误：距离测量需要选择两个点");
    }
    const Vec3* a = getPosition(manager, selection.ids[0]);
    const Vec3* b = getPosition(manager, selection.ids[1]);
    if (!a || !b) {
        spdlog::error("DimensionHandler::execute: 距离测量包含无效的顶点 id");
        return std::string("错误：无效的顶点 id");
    }
    return formatDistance(*a, *b);
}

std::string meshAngle(const MeshData& mesh, ModelLayer& manager, const Selection& selection)
{
    if (selection.type == ElementEnum::Vertex && selection.ids.size() == 3) {
        const Vec3* a = getPosition(manager, selection.ids[0]);
        const Vec3* b = getPosition(manager, selection.ids[1]);
        const Vec3* c = getPosition(manager, selection.ids[2]);
        if (a && b && c)
            return formatAngle(*a, *b, *c);
    }
    if (selection.type == ElementEnum::Edge && selection.ids.size() == 2) {
        // 边选择的 ids 为稳定局部边 id，两条边各自解析端点坐标构成方向
        ComponentData* comp = manager.findComponent(selection.component_id);
        if (comp) {
            Vec3 a0, a1, b0, b1;
            if (edgeEndpointPositions(*comp, mesh, selection.ids[0], a0, a1)
                && edgeEndpointPositions(*comp, mesh, selection.ids[1], b0, b1)) {
                std::ostringstream oss;
                oss << "Angle: " << toString(angleBetween(a1 - a0, b1 - b0)) << " deg";
                return oss.str();
            }
        }
    }
    spdlog::error("DimensionHandler::execute: 角度测量需要选择三个点或两条边");
    return std::string("错误：角度测量需要选择三个点或两条边");
}

std::string meshRadius(const MeshData&, ModelLayer& manager, const Selection& selection)
{
    if (selection.type != ElementEnum::Vertex || selection.ids.size() != 3) {
        spdlog::error("DimensionHandler::execute: 半径测量需要选择三个点");
        return std::string("错误：半径测量需要选择三个点");
    }
    const Vec3* a = getPosition(manager, selection.ids[0]);
    const Vec3* b = getPosition(manager, selection.ids[1]);
    const Vec3* c = getPosition(manager, selection.ids[2]);
    if (!a || !b || !c) {
        spdlog::error("DimensionHandler::execute: 半径测量包含无效的顶点 id");
        return std::string("错误：无效的顶点 id");
    }
    return formatRadius(*a, *b, *c);
}

std::string meshLength(const MeshData& mesh, ModelLayer& manager, const Selection& selection)
{
    // 边选择的 ids 为稳定局部边 id（每条边一个），经所属组件邻接表解析端点求长
    if (selection.type != ElementEnum::Edge || selection.ids.empty()) {
        spdlog::error("DimensionHandler::execute: 长度测量需要选择边");
        return std::string("错误：长度测量需要选择边");
    }
    ComponentData* comp = manager.findComponent(selection.component_id);
    if (!comp) {
        spdlog::error("DimensionHandler::execute: 找不到所选边所在的组件");
        return std::string("错误：找不到所选边所在的组件");
    }

    double total = 0.0;
    std::ostringstream oss;
    std::size_t edge_no = 0;
    for (Index edge_id : selection.ids) {
        Vec3 p0, p1;
        if (!edgeEndpointPositions(*comp, mesh, edge_id, p0, p1)) {
            oss << "边 " << edge_id << ": 无效边 id\n";
            continue;
        }

        const double len = length(p1 - p0);
        total += len;
        oss << "边 " << edge_no << ": " << toString(len) << "\n";
        ++edge_no;
    }

    oss << "累计长度: " << toString(total);
    return oss.str();
}

std::string meshArea(const MeshData& mesh, ModelLayer&, const Selection& selection)
{
    if (selection.type != ElementEnum::Face) {
        spdlog::error("DimensionHandler::execute: 面积测量需要选择面");
        return std::string("错误：面积测量需要选择面");
    }
    return formatFaceArea(mesh, selection.ids);
}

std::string meshVolume(const MeshData& mesh, ModelLayer&, const Selection& selection)
{
    if (selection.type != ElementEnum::Solid) {
        spdlog::error("DimensionHandler::execute: 体积测量需要选择体");
        return std::string("错误：体积测量需要选择体");
    }
    return formatSolidVolume(mesh, selection.ids);
}

std::string meshBoundingBox(const MeshData& mesh, ModelLayer& manager, const Selection& selection)
{
    const auto positions = collectPositions(mesh, manager, selection);
    if (positions.empty()) {
        spdlog::error("DimensionHandler::execute: 未找到可用顶点");
        return std::string("错误：未找到可用顶点");
    }
    return formatBoundingBox(positions);
}

std::string meshCentroid(const MeshData& mesh, ModelLayer& manager, const Selection& selection)
{
    const auto positions = collectPositions(mesh, manager, selection);
    if (positions.empty()) {
        spdlog::error("DimensionHandler::execute: 未找到可用顶点");
        return std::string("错误：未找到可用顶点");
    }
    return formatCentroid(positions);
}

//! @brief 测量操作注册表：新增测量类型只需在此加一行，args_type/校验/分发全部由表生成
const MeasureOp kMeasureOps[] = {
    { MeasureType::Distance, "距离", "Vertex", "GeometryVertex", meshDistance, geomDistance },
    { MeasureType::Angle, "角度", "Vertex,Edge", "GeometryVertex,GeometryEdge", meshAngle, geomAngle },
    { MeasureType::Radius, "半径", "Vertex", "GeometryVertex,GeometryEdge", meshRadius, geomRadius },
    { MeasureType::Length, "长度", "Edge", "GeometryEdge", meshLength, geomLength },
    { MeasureType::Area, "面积", "Face", "GeometryFace", meshArea, geomArea },
    { MeasureType::Volume, "体积", "Solid", "GeometrySolid", meshVolume, geomVolume },
    { MeasureType::BoundingBox, "包围盒", "Vertex,Edge,Face,Solid", "GeometryVertex,GeometryEdge,GeometryFace,GeometrySolid", meshBoundingBox, geomBoundingBox },
    { MeasureType::Centroid, "重心", "Vertex,Edge,Face,Solid", "GeometryVertex,GeometryEdge,GeometryFace,GeometrySolid", meshCentroid, geomCentroid },
};

//! @brief 按下标查注册表，越界返回 nullptr
const MeasureOp* findMeasureOp(int index)
{
    if (index < 0 || index >= static_cast<int>(sizeof(kMeasureOps) / sizeof(kMeasureOps[0])))
        return nullptr;
    return &kMeasureOps[index];
}

//! @brief 由注册表生成 Combo 内容（名称与下标天然一致）
std::string comboContent()
{
    std::string out;
    for (const MeasureOp& op : kMeasureOps) {
        if (!out.empty())
            out += ",";
        out += op.name;
    }
    return out;
}

//! @brief 由注册表生成选择器内容：网格模式按 Vertex,Edge,Face,Solid 规范序取并集
std::string selectorContent()
{
    static const char* canonical[] = { "Vertex", "Edge", "Face", "Solid" };
    std::string out;
    for (const char* mode : canonical) {
        bool used = false;
        for (const MeasureOp& op : kMeasureOps) {
            if (std::string(op.mesh_selector_modes).find(mode) != std::string::npos) {
                used = true;
                break;
            }
        }
        if (used) {
            if (!out.empty())
                out += ",";
            out += mode;
        }
    }
    return out;
}
}

void DimensionHandler::setup(FeatureRegistrar& reg)
{
    // Combo 内容全部由测量操作注册表生成，名称与下标天然一致
    reg.addParameter({ ArgTypeEnum::Combo, "测量类型", comboContent() + "|0" });
    reg.addParameter({ ArgTypeEnum::Selector, "选择对象", selectorContent() });
    reg.addMenuItem({ "工具", "尺寸标注" });
}

std::any DimensionHandler::execute(FeatureContext& ctx)
{
    const int* type_index = ctx.params.value(kTypeParam).get<ArgTypeEnum::Combo>();
    if (!type_index) {
        spdlog::error("DimensionHandler::execute: 测量类型参数无效");
        return std::string("错误：测量类型参数无效");
    }

    const auto selection_ptr = ctx.params.value(kSelectionParam).get<ArgTypeEnum::Selector>();
    if (!selection_ptr || !*selection_ptr) {
        spdlog::error("DimensionHandler::execute: 选择对象参数无效");
        return std::string("错误：选择对象参数无效");
    }

    const auto& selection = **selection_ptr;

    // 选择集未带组件 id 时回退当前活动组件
    Index selected_component_id = selection.component_id;
    if (selected_component_id < 0) {
        const auto active_component = ctx.activeComponent ? ctx.activeComponent() : std::nullopt;
        if (active_component) {
            selected_component_id = *active_component;
        }
    }
    ComponentData* comp = ctx.model.findComponent(selected_component_id);
    if (!comp) {
        spdlog::error("DimensionHandler::execute: 找不到选择对象所在的组件");
        return std::string("错误：找不到选择对象所在的组件");
    }
    ModelLayer& manager = ctx.model;

    // 回填组件 id，使按组件解析的测量实现（边测量）能定位所属组件
    Selection resolved_selection = selection;
    resolved_selection.component_id = selected_component_id;

    const MeasureOp* op = findMeasureOp(*type_index);
    if (!op) {
        spdlog::error("DimensionHandler::execute: 未知测量类型下标 {}", *type_index);
        return std::string("错误：未知测量类型");
    }

    MeshData* mesh = comp->asMeshData();
    if (!mesh) {
        // 无网格但有几何（STEP/IGES）：走 OCC 几何测量
        if (comp->geometry) {
            comp->geometry->ensureIndexBuilt(manager.geomRegistry());
            if (!op->geom_exec)
                return std::string("错误：") + op->name + "测量暂不支持几何模型";
            return op->geom_exec(manager.geomRegistry(), resolved_selection);
        }
        spdlog::error("DimensionHandler::execute: 选择对象所在组件没有网格或几何数据");
        return std::string("错误：选择对象所在组件没有网格或几何数据");
    }

    return op->mesh_exec(*mesh, manager, resolved_selection);
}
} // namespace systems::feature
