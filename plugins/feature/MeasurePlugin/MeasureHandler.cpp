/**
 * @file MeasureHandler.cpp
 * @brief 网格测量处理器，支持距离、角度、半径、长度、面积、体积、包围盒与重心
 * @author 范成通 email 1941804585@qq.com
 */

#include "MeasureHandler.h"
#include "ArgObject.h"
#include "ComponentData.h"
#include "FeatureContext.h"
#include "FeatureParams.h"
#include "FeatureRegistrar.h"
#include "GeometryData.h"
#include "GeometryRegistry.h"
#include "InteractionContext.h"
#include "MeshData.h"
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
#include <cassert>
#include <cmath>
#include <limits>
#include <numeric>
#include <sstream>
#include <string>

namespace systems::feature {

namespace {
using Vec3 = std::array<double, 3>;

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
using MeshExecFn = std::string (*)(const MeshData&, const ModelLayer&, const Selection&);
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

bool isValidVertexId(const MeshData& mesh, const ModelLayer& manager, Index id)
{
    if (id < 0)
        return false;
    if (mesh.local_to_global_.empty())
        return id < static_cast<Index>(mesh.vertex_positions_.size());
    return id < static_cast<Index>(manager.globalPoints().size());
}

const Vec3& getPosition(const MeshData& mesh, const ModelLayer& manager, Index id)
{
    if (!mesh.local_to_global_.empty() && !manager.globalPoints().empty())
        return manager.globalPoints()[id];
    return mesh.vertex_positions_[id];
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

double polyhedronVolume(const MeshData& mesh, const ModelLayer& manager, Index solid_id)
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

        const Vec3& v0 = getPosition(mesh, manager, mesh.solid_faces_vertices_[vert_start]);
        for (Index i = vert_start + 1; i + 1 < vert_end; ++i) {
            const Vec3& vi = getPosition(mesh, manager, mesh.solid_faces_vertices_[i]);
            const Vec3& vj = getPosition(mesh, manager, mesh.solid_faces_vertices_[i + 1]);
            volume += dot(v0, cross(vi, vj)) / 6.0;
        }
    }

    return std::abs(volume);
}

double solidVolume(const MeshData& mesh, const ModelLayer& manager, Index solid_id)
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
        const Vec3& a = getPosition(mesh, manager, mesh.solid_vertices_[start]);
        const Vec3& b = getPosition(mesh, manager, mesh.solid_vertices_[start + 1]);
        const Vec3& c = getPosition(mesh, manager, mesh.solid_vertices_[start + 2]);
        const Vec3& d = getPosition(mesh, manager, mesh.solid_vertices_[start + 3]);
        return tetraVolume(a, b, c, d);
    }

    // VTK_POLYHEDRON = 42
    if (cell_type == 42)
        return polyhedronVolume(mesh, manager, solid_id);

    return 0.0;
}

std::vector<Vec3> collectPositions(const MeshData& mesh, const ModelLayer& manager,
    ElementEnum::Type type, const std::vector<Index>& ids)
{
    std::vector<Vec3> positions;
    positions.reserve(ids.size() * 4);

    switch (type) {
    case ElementEnum::Vertex:
    case ElementEnum::Edge:
        // 边选择的 ids 即每条边端点的顶点 id（EdgeSelectorHighlight 约定），与点一样按顶点处理
        for (Index id : ids) {
            if (isValidVertexId(mesh, manager, id))
                positions.push_back(getPosition(mesh, manager, id));
        }
        break;
    case ElementEnum::Face:
        for (Index id : ids) {
            if (id + 1 >= static_cast<Index>(mesh.face_vertices_offset_.size()))
                continue;
            for (Index i = mesh.face_vertices_offset_[id]; i < mesh.face_vertices_offset_[id + 1]; ++i) {
                const Index v = mesh.face_vertices_[i];
                if (isValidVertexId(mesh, manager, v))
                    positions.push_back(getPosition(mesh, manager, v));
            }
        }
        break;
    case ElementEnum::Solid:
        for (Index id : ids) {
            if (id + 1 >= static_cast<Index>(mesh.solid_vertices_offset_.size()))
                continue;
            for (Index i = mesh.solid_vertices_offset_[id]; i < mesh.solid_vertices_offset_[id + 1]; ++i) {
                const Index v = mesh.solid_vertices_[i];
                if (isValidVertexId(mesh, manager, v))
                    positions.push_back(getPosition(mesh, manager, v));
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

std::string formatEdgeLength(const MeshData& mesh, const ModelLayer& manager, const std::vector<Index>& ids)
{
    double total = 0.0;
    std::ostringstream oss;

    // 边选择的 ids 是每条边两个端点的顶点 id 对，逐对计算长度
    for (size_t i = 0; i + 1 < ids.size(); i += 2) {
        const Index v0 = ids[i];
        const Index v1 = ids[i + 1];
        const size_t edge_no = i / 2;
        if (!isValidVertexId(mesh, manager, v0) || !isValidVertexId(mesh, manager, v1)) {
            oss << "边 " << edge_no << ": 无效顶点\n";
            continue;
        }

        const double len = length(getPosition(mesh, manager, v1) - getPosition(mesh, manager, v0));
        total += len;
        oss << "边 " << edge_no << ": " << toString(len) << "\n";
    }

    oss << "累计长度: " << toString(total);
    return oss.str();
}

std::string formatFaceArea(const MeshData& mesh, const ModelLayer& manager, const std::vector<Index>& ids)
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
            points.push_back(getPosition(mesh, manager, mesh.face_vertices_[i]));

        const double area = polygonArea(points);
        total += area;
        oss << "面 " << id << ": " << toString(area) << "\n";
    }

    oss << "总面积: " << toString(total);
    return oss.str();
}

std::string formatSolidVolume(const MeshData& mesh, const ModelLayer& manager, const std::vector<Index>& ids)
{
    double total = 0.0;
    std::ostringstream oss;

    for (Index id : ids) {
        const double volume = solidVolume(mesh, manager, id);
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

std::string meshDistance(const MeshData& mesh, const ModelLayer& manager, const Selection& selection)
{
    if (selection.type != ElementEnum::Vertex || selection.ids.size() != 2) {
        spdlog::error("MeasureHandler::execute: 距离测量需要选择两个点");
        return std::string("错误：距离测量需要选择两个点");
    }
    const Vec3& a = getPosition(mesh, manager, selection.ids[0]);
    const Vec3& b = getPosition(mesh, manager, selection.ids[1]);
    return formatDistance(a, b);
}

std::string meshAngle(const MeshData& mesh, const ModelLayer& manager, const Selection& selection)
{
    if (selection.type == ElementEnum::Vertex && selection.ids.size() == 3) {
        const Vec3& a = getPosition(mesh, manager, selection.ids[0]);
        const Vec3& b = getPosition(mesh, manager, selection.ids[1]);
        const Vec3& c = getPosition(mesh, manager, selection.ids[2]);
        return formatAngle(a, b, c);
    }
    if (selection.type == ElementEnum::Edge && selection.ids.size() == 4) {
        // 边选择的 ids 是每条边两个端点的顶点 id（EdgeSelectorHighlight 约定）
        const auto& ids = selection.ids;
        const bool valid = isValidVertexId(mesh, manager, ids[0]) && isValidVertexId(mesh, manager, ids[1])
            && isValidVertexId(mesh, manager, ids[2]) && isValidVertexId(mesh, manager, ids[3]);
        if (valid) {
            const Vec3 u = getPosition(mesh, manager, ids[1]) - getPosition(mesh, manager, ids[0]);
            const Vec3 v = getPosition(mesh, manager, ids[3]) - getPosition(mesh, manager, ids[2]);
            std::ostringstream oss;
            oss << "Angle: " << toString(angleBetween(u, v)) << " deg";
            return oss.str();
        }
    }
    spdlog::error("MeasureHandler::execute: 角度测量需要选择三个点或两条边");
    return std::string("错误：角度测量需要选择三个点或两条边");
}

std::string meshRadius(const MeshData& mesh, const ModelLayer& manager, const Selection& selection)
{
    if (selection.type != ElementEnum::Vertex || selection.ids.size() != 3) {
        spdlog::error("MeasureHandler::execute: 半径测量需要选择三个点");
        return std::string("错误：半径测量需要选择三个点");
    }
    const Vec3& a = getPosition(mesh, manager, selection.ids[0]);
    const Vec3& b = getPosition(mesh, manager, selection.ids[1]);
    const Vec3& c = getPosition(mesh, manager, selection.ids[2]);
    return formatRadius(a, b, c);
}

std::string meshLength(const MeshData& mesh, const ModelLayer& manager, const Selection& selection)
{
    // 边选择的 ids 是每条边两个端点的顶点 id 对，数量应为正偶数
    if (selection.type != ElementEnum::Edge || selection.ids.empty() || selection.ids.size() % 2 != 0) {
        spdlog::error("MeasureHandler::execute: 长度测量需要选择边");
        return std::string("错误：长度测量需要选择边");
    }
    return formatEdgeLength(mesh, manager, selection.ids);
}

std::string meshArea(const MeshData& mesh, const ModelLayer& manager, const Selection& selection)
{
    if (selection.type != ElementEnum::Face) {
        spdlog::error("MeasureHandler::execute: 面积测量需要选择面");
        return std::string("错误：面积测量需要选择面");
    }
    return formatFaceArea(mesh, manager, selection.ids);
}

std::string meshVolume(const MeshData& mesh, const ModelLayer& manager, const Selection& selection)
{
    if (selection.type != ElementEnum::Solid) {
        spdlog::error("MeasureHandler::execute: 体积测量需要选择体");
        return std::string("错误：体积测量需要选择体");
    }
    return formatSolidVolume(mesh, manager, selection.ids);
}

std::string meshBoundingBox(const MeshData& mesh, const ModelLayer& manager, const Selection& selection)
{
    const auto positions = collectPositions(mesh, manager, selection.type, selection.ids);
    if (positions.empty()) {
        spdlog::error("MeasureHandler::execute: 未找到可用顶点");
        return std::string("错误：未找到可用顶点");
    }
    return formatBoundingBox(positions);
}

std::string meshCentroid(const MeshData& mesh, const ModelLayer& manager, const Selection& selection)
{
    const auto positions = collectPositions(mesh, manager, selection.type, selection.ids);
    if (positions.empty()) {
        spdlog::error("MeasureHandler::execute: 未找到可用顶点");
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

void MeasureHandler::setup(FeatureRegistrar& reg)
{
    assertExecuteThread();
    // Combo 内容全部由测量操作注册表生成，名称与下标天然一致
    reg.addParameter({ ArgTypeEnum::Combo, "测量类型", comboContent() + "|0" });
    reg.addParameter({ ArgTypeEnum::Selector, "选择对象", selectorContent() });
    reg.addMenuItem({ "工具", "测量" });
}

std::any MeasureHandler::execute(FeatureContext& ctx)
{
    assertExecuteThread();

    const int* type_index = ctx.params.value(0).get<ArgTypeEnum::Combo>();
    if (!type_index) {
        spdlog::error("MeasureHandler::execute: 测量类型参数无效");
        return std::string("错误：测量类型参数无效");
    }

    const auto selection_ptr = ctx.params.value(1).get<ArgTypeEnum::Selector>();
    if (!selection_ptr || !*selection_ptr) {
        spdlog::error("MeasureHandler::execute: 选择对象参数无效");
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
        spdlog::error("MeasureHandler::execute: 找不到选择对象所在的组件");
        return std::string("错误：找不到选择对象所在的组件");
    }
    ModelLayer& manager = ctx.model;
    const MeasureOp* op = findMeasureOp(*type_index);
    if (!op) {
        spdlog::error("MeasureHandler::execute: 未知测量类型下标 {}", *type_index);
        return std::string("错误：未知测量类型");
    }

    MeshData* mesh = comp->asMeshData();
    if (!mesh) {
        // 无网格但有几何（STEP/IGES）：走 OCC 几何测量
        if (comp->geometry) {
            comp->geometry->ensureIndexBuilt(manager.geomRegistry());
            if (!op->geom_exec)
                return std::string("错误：") + op->name + "测量暂不支持几何模型";
            return op->geom_exec(manager.geomRegistry(), selection);
        }
        spdlog::error("MeasureHandler::execute: 选择对象所在组件没有网格或几何数据");
        return std::string("错误：选择对象所在组件没有网格或几何数据");
    }

    return op->mesh_exec(*mesh, manager, selection);
}

void MeasureHandler::activate(FeatureContext& ctx)
{
    assertExecuteThread();
    ctx.interaction.onActivate([this]() { this->clear(); });
    ctx.interaction.onDeactivate([this]() { this->clear(); });
    ctx.interaction.onPick([this](const PickInfo& p) { return this->onPick(p); });
    ctx.interaction.onHover([this](const PickInfo& p) { return this->onHover(p); });
}
// ---------------- 交互回调（经 InteractionContext 注册） ----------------

namespace {
constexpr double kEps = 1e-9;
}

void MeasureHandler::assertExecuteThread() const
{
    assert(std::this_thread::get_id() == gui_thread_id_);
}

void MeasureHandler::assertInteractiveThread() const
{
    if (interactive_thread_id_ == std::thread::id())
        interactive_thread_id_ = std::this_thread::get_id();
    assert(std::this_thread::get_id() == interactive_thread_id_);
}

bool MeasureHandler::samePoint(const PickInfo& a, const PickInfo& b)
{
    if (a.mesh_id >= 0 && a.mesh_id == b.mesh_id)
        return true;
    if (a.geom_id >= 0 && a.geom_id == b.geom_id)
        return true;
    // 同源顶点再次拾取坐标位级一致，作无 id 时的兜底
    return a.world_pos == b.world_pos;
}

bool MeasureHandler::onPick(const PickInfo& pick)
{
    assertInteractiveThread();
    if (!pick.valid)
        return false;

    if (!pending_) {
        pending_ = pick;
    } else if (samePoint(*pending_, pick)) {
        pending_.reset(); // 同一点再点一次 = 取消本次起笔
    } else {
        addLine(*pending_, pick);
        pending_.reset();
    }

    refreshAnnotations();
    return true;
}

bool MeasureHandler::onHover(const PickInfo& pick)
{
    assertInteractiveThread();
    // 未吸附或无起笔：清除已有预览；本来无预览则无需刷新
    if (!pending_ || !pick.valid) {
        if (!has_preview_)
            return false;
        has_preview_ = false;
        refreshAnnotations();
        return true;
    }

    has_preview_ = true;
    preview_ = pick;
    refreshAnnotations();
    return true;
}

void MeasureHandler::addLine(const PickInfo& a, const PickInfo& b)
{
    // 与已有线完全重复（含反向）则忽略
    for (const MeasureLine& l : lines_) {
        if ((samePoint(l.a, a) && samePoint(l.b, b)) || (samePoint(l.a, b) && samePoint(l.b, a)))
            return;
    }

    // 与每条已有线做端点匹配，共端点即记录一组夹角
    const int new_idx = static_cast<int>(lines_.size());
    for (size_t i = 0; i < lines_.size(); ++i) {
        const MeasureLine& l = lines_[i];
        auto try_share = [&](const PickInfo& old_shared, const PickInfo& old_other,
                             const PickInfo& new_shared, const PickInfo& new_other) {
            if (!samePoint(old_shared, new_shared))
                return;
            MeasureAngle ang;
            ang.line1 = static_cast<int>(i);
            ang.line2 = new_idx;
            ang.at = old_shared.world_pos;
            ang.p = old_other.world_pos;
            ang.q = new_other.world_pos;
            ang.angle = angleBetween(ang.p - ang.at, ang.q - ang.at);
            angles_.push_back(ang);
        };
        try_share(l.a, l.b, a, b);
        try_share(l.a, l.b, b, a);
        try_share(l.b, l.a, a, b);
        try_share(l.b, l.a, b, a);
    }
    lines_.push_back({ a, b });
}

void MeasureHandler::refreshAnnotations()
{
    annotations_.clear();

    // 已确认线的端点与线段（红色端点、绿色实线）
    for (const MeasureLine& l : lines_) {
        annotations_.points.push_back({ l.a.world_pos });
        annotations_.points.push_back({ l.b.world_pos });
        annotations_.lines.push_back({ l.a.world_pos, l.b.world_pos });
    }
    if (pending_)
        annotations_.points.push_back({ pending_->world_pos });

    // 长度文本：每线一个，放线段中点（白色）
    for (const MeasureLine& l : lines_) {
        const Vec3 mid = midpoint(l.a.world_pos, l.b.world_pos);
        annotations_.texts.push_back({ mid, "L: " + toString(length(l.b.world_pos - l.a.world_pos), 2), 1.0, 1.0, 1.0 });
    }

    // 夹角文本：放共点沿角平分线偏移（青色）；同一点多个夹角按序号加大偏移防重叠
    for (size_t ai = 0; ai < angles_.size(); ++ai) {
        const MeasureAngle& ang = angles_[ai];
        int stack = 0;
        for (size_t j = 0; j < ai; ++j) {
            if (angles_[j].at == ang.at)
                ++stack;
        }

        const Vec3 u = ang.p - ang.at;
        const Vec3 v = ang.q - ang.at;
        const double lu = length(u);
        const double lv = length(v);
        Vec3 dir { 0.0, 0.0, 0.0 };
        if (lu > kEps && lv > kEps) {
            // 角平分线方向；u、v 近反向（180°）时退化为两端点中点方向
            const Vec3 s { u[0] / lu + v[0] / lv, u[1] / lu + v[1] / lv, u[2] / lu + v[2] / lv };
            const double ls = length(s);
            if (ls > kEps) {
                dir = { s[0] / ls, s[1] / ls, s[2] / ls };
            } else {
                const Vec3 d = midpoint(ang.p, ang.q) - ang.at;
                const double ld = length(d);
                dir = ld > kEps ? Vec3 { d[0] / ld, d[1] / ld, d[2] / ld }
                                : Vec3 { u[0] / lu, u[1] / lu, u[2] / lu };
            }
        }
        const double dist = 0.25 * std::min(lu, lv) * (1.0 + 0.3 * stack);
        annotations_.texts.push_back({ { ang.at[0] + dir[0] * dist, ang.at[1] + dir[1] * dist,
                                           ang.at[2] + dir[2] * dist },
            "Ang: " + toString(ang.angle, 2), 0.3, 0.9, 1.0 });
    }

    // 悬停动态预览：黄色虚线 + 黄色长度文本
    if (pending_ && has_preview_) {
        AnnotationLine preview;
        preview.p0 = pending_->world_pos;
        preview.p1 = preview_.world_pos;
        preview.r = 1.0;
        preview.g = 0.9;
        preview.b = 0.1;
        preview.dashed = true;
        annotations_.lines.push_back(preview);

        const Vec3 mid = midpoint(pending_->world_pos, preview_.world_pos);
        annotations_.texts.push_back({ mid,
            "L: " + toString(length(preview_.world_pos - pending_->world_pos), 2), 1.0, 0.9, 0.1 });
    }
}

std::string MeasureHandler::resultText() const
{
    assertInteractiveThread();
    if (lines_.empty() && !pending_)
        return {};

    std::string out = "已完成直线: " + std::to_string(lines_.size()) + " 条";
    if (pending_)
        out += "（已选第 1 点，再点第 2 点成线）";
    for (size_t i = 0; i < lines_.size(); ++i) {
        out += "\nL" + std::to_string(i + 1) + ": "
            + toString(length(lines_[i].b.world_pos - lines_[i].a.world_pos), 6);
    }
    for (const MeasureAngle& ang : angles_) {
        out += "\n夹角(L" + std::to_string(ang.line1 + 1) + ",L" + std::to_string(ang.line2 + 1)
            + "): " + toString(ang.angle, 6);
    }
    return out;
}

void MeasureHandler::clear()
{
    assertInteractiveThread();
    pending_.reset();
    has_preview_ = false;
    lines_.clear();
    angles_.clear();
    refreshAnnotations();
}

} // namespace systems::feature
