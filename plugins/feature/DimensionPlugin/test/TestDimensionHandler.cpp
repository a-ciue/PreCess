/**
 * @file TestDimensionHandler.cpp
 * @brief 尺寸标注处理器的单元测试
 * @author 范成通 email 1941804585@qq.com
 */

#include "DimensionHandler.h"
#include "ArgObject.h"
#include "ComponentData.h"
#include "EventBus.h"
#include "FeatureContext.h"
#include "FeatureEventGateway.h"
#include "FeatureParams.h"
#include "FeatureRegistrar.h"
#include "GeometryData.h"
#include "InteractionContext.h"
#include "InteractionState.h"
#include "MakeMeshData.h"
#include "MeshAdjacency.h"
#include "MeshData.h"
#include "ModelLayer.h"
#include "Selection.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRep_Tool.hxx>
#include <GeomAbs_CurveType.hxx>
#include <TopExp.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Vertex.hxx>
#include <gp.hxx>
#include <gp_Pnt.hxx>

#include <catch2/catch_test_macros.hpp>

#include <any>
#include <array>
#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <vector>

using namespace systems::feature;

namespace {

std::shared_ptr<Selection> makeVertexSelection(const std::vector<Index>& ids)
{
    return std::make_shared<Selection>(Selection { ids, ElementEnum::Vertex, 0 });
}

std::shared_ptr<Selection> makeEdgeSelection(const std::vector<Index>& ids)
{
    return std::make_shared<Selection>(Selection { ids, ElementEnum::Edge, 0 });
}

std::shared_ptr<Selection> makeFaceSelection(const std::vector<Index>& ids)
{
    return std::make_shared<Selection>(Selection { ids, ElementEnum::Face, 0 });
}

std::shared_ptr<Selection> makeSolidSelection(const std::vector<Index>& ids)
{
    return std::make_shared<Selection>(Selection { ids, ElementEnum::Solid, 0 });
}

std::shared_ptr<Selection> makeGeometrySelection(ElementEnum::Type type, const std::vector<Index>& ids)
{
    return std::make_shared<Selection>(Selection { ids, type, 0 });
}

//! @brief 功能测试环境：手工装配 FeatureContext（参数集 + 活动组件 provider），替代原算法 HandlerContext
struct FeatureTestEnv {
    ModelLayer mgr;
    core::EventBus bus;
    FeatureEventGateway gateway { bus, mgr }; //> 事件网关（声明顺序须在 mgr/bus 之后）
    DimensionHandler handler;
    FeatureRegistrar registrar;
    std::optional<Index> active_component;
    std::unique_ptr<FeatureParams> params;
    systems::interaction::InteractionState interaction_state_;
    InteractionContext interaction_ctx_;
    std::unique_ptr<FeatureContext> ctx;

    FeatureTestEnv()
        : interaction_ctx_(interaction_state_)
    {
        // 装配顺序同 FeatureSystem：先空参数集装配 ctx，setup 收集声明后载入默认值
        params = std::make_unique<FeatureParams>(std::vector<core::ArgType> {});
        ctx = std::make_unique<FeatureContext>(FeatureContext {
            mgr,
            gateway,
            *params,
            interaction_ctx_,
            []() -> std::optional<Index> { return std::nullopt; },
            [this]() { return active_component; },
            [this](Index component_id) { return mgr.getComponentOperator(component_id); },
        });
        // 参数声明直接取自被测功能的 setup，避免测试中重复维护一份
        handler.setup(registrar, *ctx);
        *params = FeatureParams(registrar.argTypes());
    }

    //! @brief 写入功能参数并执行尺寸标注，返回结果字符串（下标 0=测量类型 1=选择对象，见 setup）
    std::string executeDimension(int measure_type_index, std::shared_ptr<Selection> selection)
    {
        params->setValue(0, core::ArgObject::create<ArgTypeEnum::Combo>(measure_type_index));
        params->setValue(1, core::ArgObject::create<ArgTypeEnum::Selector>(std::move(selection)));
        auto result_any = handler.execute(*ctx);
        if (const std::string* result = std::any_cast<std::string>(&result_any))
            return *result;
        return {};
    }
};

//! @brief 向模型层添加一个 MakeMeshData 网格组件
//! @return 组件 id；comp_out 出参接收组件指针（失败时为 nullptr）
Index addMakeMeshComponent(ModelLayer& mgr, ComponentData** comp_out)
{
    auto mesh = std::make_unique<MeshData>(MakeMeshData());
    auto c = std::make_unique<ComponentData>();
    c->id = -1;
    c->name = "Comp";
    c->mesh = std::move(mesh);

    ComponentDatas comps;
    comps.push_back(std::move(c));

    const Index model_id = mgr.addModel("test", std::move(comps));
    auto cids = mgr.modelById(model_id)->componentIds();
    if (cids.size() != 1) {
        *comp_out = nullptr;
        return -1;
    }
    *comp_out = mgr.findComponent(cids[0]);
    return cids[0];
}

//! @brief 在 10×10×10 的 OCC 立方体几何组件上执行一次测量，返回结果字符串
std::string executeGeometryMeasureOnBox(int measure_type_index, ElementEnum::Type sel_type, int local_id)
{
    BRepPrimAPI_MakeBox box_maker(10.0, 10.0, 10.0);
    box_maker.Build();
    if (!box_maker.IsDone())
        return {};

    auto geometry = std::make_unique<GeometryData>();
    geometry->rootShape = std::make_unique<TopoDS_Shape>(box_maker.Shape());

    auto c = std::make_unique<ComponentData>();
    c->id = -1;
    c->name = "box";
    c->geometry = std::move(geometry);

    ComponentDatas comps;
    comps.push_back(std::move(c));

    FeatureTestEnv env;
    const Index model_id = env.mgr.addModel("box", std::move(comps));
    auto cids = env.mgr.modelById(model_id)->componentIds();
    if (cids.size() != 1)
        return {};

    ComponentData* comp = env.mgr.findComponent(cids[0]);
    if (!comp)
        return {};
    comp->geometry->ensureIndexBuilt(env.mgr.geomRegistry());

    Index gid = -1;
    switch (sel_type) {
    case ElementEnum::GeometryEdge:
        gid = comp->geometry->index.edgeGlobalId(local_id);
        break;
    case ElementEnum::GeometryFace:
        gid = comp->geometry->index.faceGlobalId(local_id);
        break;
    case ElementEnum::GeometrySolid:
        gid = comp->geometry->index.solidGlobalId(local_id);
        break;
    default:
        break;
    }

    auto selection = makeGeometrySelection(sel_type, { gid });
    selection->component_id = cids[0];
    return env.executeDimension(measure_type_index, selection);
}

/**
 * @brief 在任意 OCC 几何组件上执行一次测量（支持多 id 选择）
 *
 * @return 结果字符串；几何构建失败或 local id 无效返回空串
 */
std::string executeGeometryMeasure(const TopoDS_Shape& shape, int measure_type_index,
    ElementEnum::Type sel_type, const std::vector<int>& local_ids)
{
    auto geometry = std::make_unique<GeometryData>();
    geometry->rootShape = std::make_unique<TopoDS_Shape>(shape);

    auto c = std::make_unique<ComponentData>();
    c->id = -1;
    c->name = "geom";
    c->geometry = std::move(geometry);

    ComponentDatas comps;
    comps.push_back(std::move(c));

    FeatureTestEnv env;
    const Index model_id = env.mgr.addModel("geom", std::move(comps));
    auto cids = env.mgr.modelById(model_id)->componentIds();
    if (cids.size() != 1)
        return {};

    ComponentData* comp = env.mgr.findComponent(cids[0]);
    if (!comp)
        return {};
    comp->geometry->ensureIndexBuilt(env.mgr.geomRegistry());

    // 按选择类型将局部 id 换算为几何全局 id
    std::vector<Index> gids;
    for (int local_id : local_ids) {
        Index gid = -1;
        switch (sel_type) {
        case ElementEnum::GeometryVertex:
            gid = comp->geometry->index.vertexGlobalId(local_id);
            break;
        case ElementEnum::GeometryEdge:
            gid = comp->geometry->index.edgeGlobalId(local_id);
            break;
        case ElementEnum::GeometryFace:
            gid = comp->geometry->index.faceGlobalId(local_id);
            break;
        case ElementEnum::GeometrySolid:
            gid = comp->geometry->index.solidGlobalId(local_id);
            break;
        default:
            break;
        }
        if (gid < 0)
            return {};
        gids.push_back(gid);
    }

    auto selection = makeGeometrySelection(sel_type, gids);
    selection->component_id = cids[0];
    return env.executeDimension(measure_type_index, selection);
}

//! @brief 10×10×10 立方体几何测量的便捷包装
std::string executeGeometryMeasureOnBox(int measure_type_index,
    ElementEnum::Type sel_type, const std::vector<int>& local_ids)
{
    BRepPrimAPI_MakeBox box_maker(10.0, 10.0, 10.0);
    box_maker.Build();
    if (!box_maker.IsDone())
        return {};
    return executeGeometryMeasure(box_maker.Shape(), measure_type_index, sel_type, local_ids);
}

// ---------------- 几何拓扑枚举辅助（与 GeometrySubshapeIndex 同为 TopExp::MapShapes，local id 一致） ----------------

std::array<double, 3> toArr(const gp_Pnt& p)
{
    return { p.X(), p.Y(), p.Z() };
}

double pointDist(const std::array<double, 3>& a, const std::array<double, 3>& b)
{
    return std::sqrt((a[0] - b[0]) * (a[0] - b[0])
        + (a[1] - b[1]) * (a[1] - b[1])
        + (a[2] - b[2]) * (a[2] - b[2]));
}

double dot3(const std::array<double, 3>& a, const std::array<double, 3>& b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

//! @brief 按与 GeometrySubshapeIndex 相同的枚举顺序取几何顶点坐标（local id 从 1 起）
std::vector<std::array<double, 3>> listVertexPoints(const TopoDS_Shape& shape)
{
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> map;
    TopExp::MapShapes(shape, TopAbs_VERTEX, map);

    std::vector<std::array<double, 3>> points(static_cast<size_t>(map.Extent() + 1));
    for (int i = 1; i <= map.Extent(); ++i)
        points[static_cast<size_t>(i)] = toArr(BRep_Tool::Pnt(TopoDS::Vertex(map(i))));
    return points;
}

//! @brief 直线边方向（首末顶点）；退化边返回零向量
std::array<double, 3> lineEdgeDirection(const TopoDS_Shape& edge)
{
    TopoDS_Vertex v_first, v_last;
    TopExp::Vertices(TopoDS::Edge(edge), v_first, v_last);
    const auto p0 = toArr(BRep_Tool::Pnt(v_first));
    const auto p1 = toArr(BRep_Tool::Pnt(v_last));
    return { p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2] };
}

} // namespace

TEST_CASE("DimensionHandler setup declares measure parameters and menu")
{
    FeatureTestEnv env;

    REQUIRE(env.registrar.argTypes().size() == 2);
    CHECK(env.registrar.argTypes()[0].type == ArgTypeEnum::Combo);
    CHECK(env.registrar.argTypes()[1].type == ArgTypeEnum::Selector);
    REQUIRE(env.registrar.menuItems().size() == 1);
}

TEST_CASE("DimensionHandler: distance between two vertices")
{
    auto mesh = std::make_unique<MeshData>(MakeMeshData());
    auto c = std::make_unique<ComponentData>();
    c->id = -1;
    c->name = "Comp";
    c->mesh = std::move(mesh);

    ComponentDatas comps;
    comps.push_back(std::move(c));

    FeatureTestEnv env;
    const Index model_id = env.mgr.addModel("test", std::move(comps));

    auto cids = env.mgr.modelById(model_id)->componentIds();
    REQUIRE(cids.size() == 1);

    ComponentData* comp = env.mgr.findComponent(cids[0]);
    REQUIRE(comp);

    MeshData* mesh_ptr = comp->asMeshData();
    REQUIRE(mesh_ptr);

    // 选择集顶点 id 为全局点 id，取组件入池时分配的真实 gid
    auto selection = makeVertexSelection({ comp->point_global_ids_[0], comp->point_global_ids_[1] });
    selection->component_id = cids[0];
    const std::string result = env.executeDimension(0, selection); // 距离
    CHECK(result.find("Distance") != std::string::npos);
    CHECK(result.find("1.000000") != std::string::npos);
}

TEST_CASE("DimensionHandler: length of edges selected by stable edge ids")
{
    auto mesh = std::make_unique<MeshData>(MakeMeshData());
    auto c = std::make_unique<ComponentData>();
    c->id = -1;
    c->name = "Comp";
    c->mesh = std::move(mesh);

    ComponentDatas comps;
    comps.push_back(std::move(c));

    FeatureTestEnv env;
    const Index model_id = env.mgr.addModel("test", std::move(comps));

    auto cids = env.mgr.modelById(model_id)->componentIds();
    REQUIRE(cids.size() == 1);

    ComponentData* comp = env.mgr.findComponent(cids[0]);
    REQUIRE(comp);

    MeshData* mesh_ptr = comp->asMeshData();
    REQUIRE(mesh_ptr);

    // 边选择的 ids 为稳定局部边 id（Selection.h 契约）：边 (12,13) 与 (0,1) 长度均为 1
    MeshAdjacency& adj = comp->mesh_adjacency;
    const auto e0 = adj.findEdgeByEndpoints(*mesh_ptr, 12, 13);
    const auto e1 = adj.findEdgeByEndpoints(*mesh_ptr, 0, 1);
    REQUIRE(e0.has_value());
    REQUIRE(e1.has_value());
    const auto sid0 = adj.edgeStableId(*mesh_ptr, *e0);
    const auto sid1 = adj.edgeStableId(*mesh_ptr, *e1);
    REQUIRE(sid0.has_value());
    REQUIRE(sid1.has_value());

    auto selection = makeEdgeSelection({ *sid0, *sid1 });
    selection->component_id = cids[0];
    const std::string result = env.executeDimension(3, selection); // 长度
    CHECK(result.find("累计长度: 2.000000") != std::string::npos);
}

TEST_CASE("DimensionHandler: angle between two edges selected by stable edge ids")
{
    auto mesh = std::make_unique<MeshData>(MakeMeshData());
    auto c = std::make_unique<ComponentData>();
    c->id = -1;
    c->name = "Comp";
    c->mesh = std::move(mesh);

    ComponentDatas comps;
    comps.push_back(std::move(c));

    FeatureTestEnv env;
    const Index model_id = env.mgr.addModel("test", std::move(comps));

    auto cids = env.mgr.modelById(model_id)->componentIds();
    REQUIRE(cids.size() == 1);

    ComponentData* comp = env.mgr.findComponent(cids[0]);
    REQUIRE(comp);

    MeshData* mesh_ptr = comp->asMeshData();
    REQUIRE(mesh_ptr);

    // 边 (0,1) 方向 (1,0,0)，边 (0,3) 方向 (0,1,0)，夹角 90°（ids 为稳定局部边 id）
    MeshAdjacency& adj = comp->mesh_adjacency;
    const auto e0 = adj.findEdgeByEndpoints(*mesh_ptr, 0, 1);
    const auto e1 = adj.findEdgeByEndpoints(*mesh_ptr, 0, 3);
    REQUIRE(e0.has_value());
    REQUIRE(e1.has_value());
    const auto sid0 = adj.edgeStableId(*mesh_ptr, *e0);
    const auto sid1 = adj.edgeStableId(*mesh_ptr, *e1);
    REQUIRE(sid0.has_value());
    REQUIRE(sid1.has_value());

    auto selection = makeEdgeSelection({ *sid0, *sid1 });
    selection->component_id = cids[0];
    const std::string result = env.executeDimension(1, selection); // 角度
    CHECK(result.find("Angle: 90.000000 deg") != std::string::npos);
}

TEST_CASE("DimensionHandler: geometry edge length on OCC box")
{
    // 10×10×10 立方体任一条几何边长度均为 10
    const std::string result = executeGeometryMeasureOnBox(3, ElementEnum::GeometryEdge, 1);
    CHECK(result.find("累计长度: 10.000000") != std::string::npos);
}

TEST_CASE("DimensionHandler: geometry face area on OCC box")
{
    // 10×10×10 立方体任一个几何面面积均为 100
    const std::string result = executeGeometryMeasureOnBox(4, ElementEnum::GeometryFace, 1);
    CHECK(result.find("总面积: 100.000000") != std::string::npos);
}

TEST_CASE("DimensionHandler: geometry solid volume on OCC box")
{
    // 10×10×10 立方体体积为 1000
    const std::string result = executeGeometryMeasureOnBox(5, ElementEnum::GeometrySolid, 1);
    CHECK(result.find("总体积: 1000.000000") != std::string::npos);
}

// ---------------- 网格测量：角度（三点）/半径/面积/体积/包围盒/重心 ----------------

TEST_CASE("DimensionHandler: angle between three vertices")
{
    FeatureTestEnv env;
    ComponentData* comp = nullptr;
    const Index component_id = addMakeMeshComponent(env.mgr, &comp);
    REQUIRE(comp);

    // 以点 0 为顶点，点 1 与点 3 方向正交，夹角 90°（ids 为全局点 id）
    auto selection = makeVertexSelection({ comp->point_global_ids_[1],
        comp->point_global_ids_[0], comp->point_global_ids_[3] });
    selection->component_id = component_id;
    const std::string result = env.executeDimension(1, selection); // 角度
    CHECK(result.find("Angle: 90.000000 deg") != std::string::npos);
}

TEST_CASE("DimensionHandler: radius through three vertices")
{
    FeatureTestEnv env;
    ComponentData* comp = nullptr;
    const Index component_id = addMakeMeshComponent(env.mgr, &comp);
    REQUIRE(comp);

    // 直角三角形 (0,0,0),(1,0,0),(0,1,0)：外接圆半径 = 斜边/2 = √2/2
    auto selection = makeVertexSelection({ comp->point_global_ids_[0],
        comp->point_global_ids_[1], comp->point_global_ids_[3] });
    selection->component_id = component_id;
    const std::string result = env.executeDimension(2, selection); // 半径
    CHECK(result.find("Radius: 0.707107") != std::string::npos);
}

TEST_CASE("DimensionHandler: area of selected faces")
{
    FeatureTestEnv env;
    ComponentData* comp = nullptr;
    const Index component_id = addMakeMeshComponent(env.mgr, &comp);
    REQUIRE(comp);

    // 面 0 为单位正方形，面积 1
    auto selection = makeFaceSelection({ 0 });
    selection->component_id = component_id;
    const std::string result = env.executeDimension(4, selection); // 面积
    CHECK(result.find("总面积: 1.000000") != std::string::npos);
}

TEST_CASE("DimensionHandler: volume of tetrahedron solid")
{
    // 单位四面体（VTK_TETRA）体积 1/6，验证体测量的成功路径
    MeshData mesh;
    mesh.init();
    mesh.vertex_positions_ = { { 0.0, 0.0, 0.0 }, { 1.0, 0.0, 0.0 },
        { 0.0, 1.0, 0.0 }, { 0.0, 0.0, 1.0 } };
    mesh.solid_types_ = { 10 };
    mesh.solid_vertices_ = { 0, 1, 2, 3 };
    mesh.solid_vertices_offset_ = { 0, 4 };

    auto c = std::make_unique<ComponentData>();
    c->id = -1;
    c->name = "Comp";
    c->mesh = std::make_unique<MeshData>(std::move(mesh));

    ComponentDatas comps;
    comps.push_back(std::move(c));

    FeatureTestEnv env;
    const Index model_id = env.mgr.addModel("test", std::move(comps));
    auto cids = env.mgr.modelById(model_id)->componentIds();
    REQUIRE(cids.size() == 1);

    auto selection = makeSolidSelection({ 0 });
    selection->component_id = cids[0];
    const std::string result = env.executeDimension(5, selection); // 体积
    CHECK(result.find("总体积: 0.166667") != std::string::npos);
}

TEST_CASE("DimensionHandler: volume reports unsupported solid types")
{
    // MakeMeshData 的体为六面体/楔体，暂不支持体积计算，须给出明确提示而非错误数值
    FeatureTestEnv env;
    ComponentData* comp = nullptr;
    const Index component_id = addMakeMeshComponent(env.mgr, &comp);
    REQUIRE(comp);

    auto selection = makeSolidSelection({ 0 });
    selection->component_id = component_id;
    const std::string result = env.executeDimension(5, selection); // 体积
    CHECK(result.find("不支持的体类型或无体积") != std::string::npos);
}

TEST_CASE("DimensionHandler: bounding box of edges selected by stable edge ids")
{
    FeatureTestEnv env;
    ComponentData* comp = nullptr;
    const Index component_id = addMakeMeshComponent(env.mgr, &comp);
    REQUIRE(comp);

    MeshData* mesh_ptr = comp->asMeshData();
    REQUIRE(mesh_ptr);

    // 独立线段 (12,13)：端点 (0,1.5,0)、(1,1.5,0)
    const auto edge = comp->mesh_adjacency.findEdgeByEndpoints(*mesh_ptr, 12, 13);
    REQUIRE(edge.has_value());
    const auto sid = comp->mesh_adjacency.edgeStableId(*mesh_ptr, *edge);
    REQUIRE(sid.has_value());

    auto selection = makeEdgeSelection({ *sid });
    selection->component_id = component_id;
    const std::string result = env.executeDimension(6, selection); // 包围盒
    CHECK(result.find("min: (0.000000, 1.500000, 0.000000)") != std::string::npos);
    CHECK(result.find("max: (1.000000, 1.500000, 0.000000)") != std::string::npos);
    CHECK(result.find("size: (1.000000, 0.000000, 0.000000)") != std::string::npos);
}

TEST_CASE("DimensionHandler: bounding box of selected faces")
{
    FeatureTestEnv env;
    ComponentData* comp = nullptr;
    const Index component_id = addMakeMeshComponent(env.mgr, &comp);
    REQUIRE(comp);

    // 面 0 为单位正方形（z=0 平面）
    auto selection = makeFaceSelection({ 0 });
    selection->component_id = component_id;
    const std::string result = env.executeDimension(6, selection); // 包围盒
    CHECK(result.find("size: (1.000000, 1.000000, 0.000000)") != std::string::npos);
}

TEST_CASE("DimensionHandler: centroid of edges selected by stable edge ids")
{
    FeatureTestEnv env;
    ComponentData* comp = nullptr;
    const Index component_id = addMakeMeshComponent(env.mgr, &comp);
    REQUIRE(comp);

    MeshData* mesh_ptr = comp->asMeshData();
    REQUIRE(mesh_ptr);

    // 独立线段 (12,13) 两端点的中点即重心
    const auto edge = comp->mesh_adjacency.findEdgeByEndpoints(*mesh_ptr, 12, 13);
    REQUIRE(edge.has_value());
    const auto sid = comp->mesh_adjacency.edgeStableId(*mesh_ptr, *edge);
    REQUIRE(sid.has_value());

    auto selection = makeEdgeSelection({ *sid });
    selection->component_id = component_id;
    const std::string result = env.executeDimension(7, selection); // 重心
    CHECK(result.find("重心: (0.500000, 1.500000, 0.000000)") != std::string::npos);
}

// ---------------- 几何测量：距离/角度（三点、两边）/半径/包围盒/重心 ----------------

TEST_CASE("DimensionHandler: geometry distance between two vertices on OCC box")
{
    BRepPrimAPI_MakeBox box_maker(10.0, 10.0, 10.0);
    box_maker.Build();
    REQUIRE(box_maker.IsDone());
    const TopoDS_Shape box = box_maker.Shape();

    // 按索引序找距离最远的一对顶点（立方体对角线 10√3）
    const auto points = listVertexPoints(box);
    int id_a = -1, id_b = -1;
    double best = 0.0;
    for (size_t i = 1; i < points.size(); ++i) {
        for (size_t j = i + 1; j < points.size(); ++j) {
            const double d = pointDist(points[i], points[j]);
            if (d > best) {
                best = d;
                id_a = static_cast<int>(i);
                id_b = static_cast<int>(j);
            }
        }
    }
    REQUIRE(id_a > 0);

    const std::string result = executeGeometryMeasure(box, 0,
        ElementEnum::GeometryVertex, { id_a, id_b }); // 距离
    CHECK(result.find("Distance: 17.320508") != std::string::npos);
}

TEST_CASE("DimensionHandler: geometry angle between three vertices on OCC box")
{
    BRepPrimAPI_MakeBox box_maker(10.0, 10.0, 10.0);
    box_maker.Build();
    REQUIRE(box_maker.IsDone());
    const TopoDS_Shape box = box_maker.Shape();

    // 找一个角点及其两个相邻顶点（距离恰为棱长 10），夹角 90°
    const auto points = listVertexPoints(box);
    int corner = -1, adj1 = -1, adj2 = -1;
    for (size_t i = 1; i < points.size() && corner < 0; ++i) {
        std::vector<int> neighbors;
        for (size_t j = 1; j < points.size(); ++j) {
            if (j != i && std::abs(pointDist(points[i], points[j]) - 10.0) < 1e-9)
                neighbors.push_back(static_cast<int>(j));
        }
        if (neighbors.size() >= 2) {
            corner = static_cast<int>(i);
            adj1 = neighbors[0];
            adj2 = neighbors[1];
        }
    }
    REQUIRE(corner > 0);

    const std::string result = executeGeometryMeasure(box, 1,
        ElementEnum::GeometryVertex, { adj1, corner, adj2 }); // 角度
    CHECK(result.find("Angle: 90.000000 deg") != std::string::npos);
}

TEST_CASE("DimensionHandler: geometry angle between two edges on OCC box")
{
    BRepPrimAPI_MakeBox box_maker(10.0, 10.0, 10.0);
    box_maker.Build();
    REQUIRE(box_maker.IsDone());
    const TopoDS_Shape box = box_maker.Shape();

    // 按索引序找一对正交的直线边
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> map;
    TopExp::MapShapes(box, TopAbs_EDGE, map);
    int id_a = -1, id_b = -1;
    for (int i = 1; i <= map.Extent() && id_a < 0; ++i) {
        const auto u = lineEdgeDirection(map(i));
        for (int j = i + 1; j <= map.Extent(); ++j) {
            const auto v = lineEdgeDirection(map(j));
            if (std::abs(dot3(u, v)) < 1e-9) {
                id_a = i;
                id_b = j;
                break;
            }
        }
    }
    REQUIRE(id_a > 0);

    const std::string result = executeGeometryMeasure(box, 1,
        ElementEnum::GeometryEdge, { id_a, id_b }); // 角度
    CHECK(result.find("Angle: 90.000000 deg") != std::string::npos);
}

TEST_CASE("DimensionHandler: geometry radius rejects straight edge")
{
    // 立方体无圆弧边：半径测量须给出明确提示而非错误数值
    const std::string result = executeGeometryMeasureOnBox(2, ElementEnum::GeometryEdge, { 1 });
    CHECK(result.find("所选边不是圆弧") != std::string::npos);
}

TEST_CASE("DimensionHandler: geometry radius of circular edge on OCC cylinder")
{
    // 轴向 Z（gp::ZOX 为 gp_Ax2 常量），半径 2、高 5
    BRepPrimAPI_MakeCylinder cyl_maker(gp::ZOX(), 2.0, 5.0);
    cyl_maker.Build();
    REQUIRE(cyl_maker.IsDone());
    const TopoDS_Shape cyl = cyl_maker.Shape();

    // 找第一条圆弧边（圆柱上下底面各有一条），半径应为 2
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> map;
    TopExp::MapShapes(cyl, TopAbs_EDGE, map);
    int circle_id = -1;
    for (int i = 1; i <= map.Extent(); ++i) {
        BRepAdaptor_Curve curve(TopoDS::Edge(map(i)));
        if (curve.GetType() == GeomAbs_Circle) {
            circle_id = i;
            break;
        }
    }
    REQUIRE(circle_id > 0);

    const std::string result = executeGeometryMeasure(cyl, 2,
        ElementEnum::GeometryEdge, { circle_id }); // 半径
    CHECK(result.find("Radius: 2.000000") != std::string::npos);
}

TEST_CASE("DimensionHandler: geometry bounding box on OCC box")
{
    const std::string result = executeGeometryMeasureOnBox(6, ElementEnum::GeometrySolid, { 1 });
    CHECK(result.find("min: (0.000000, 0.000000, 0.000000)") != std::string::npos);
    CHECK(result.find("max: (10.000000, 10.000000, 10.000000)") != std::string::npos);
}

TEST_CASE("DimensionHandler: geometry centroid on OCC box")
{
    const std::string result = executeGeometryMeasureOnBox(7, ElementEnum::GeometrySolid, { 1 });
    CHECK(result.find("重心: (5.000000, 5.000000, 5.000000)") != std::string::npos);
}
