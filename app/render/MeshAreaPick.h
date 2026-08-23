// SPDX-License-Identifier: AGPL-3.0-or-later
/**
 * @file MeshAreaPick.h
 * @brief 网格框选的拾取与隔离工具（header-only）
 *
 * 设计原则（来自 D:\PreCessDemo\AreaPickDemo.cpp，**逐行移植**，不"优化"）：
 *   - 调 vtkHardwareSelector 选目标 actor 自身，避免穿透到其它 actor。
 *   - 通过临时 visibility 开关把场景里其它 actor 隔离掉；keepVisible 列表内的 actor
 *     保持可见并填充 z-buffer，使 POINTS/EDGES 模式不会拾到背后的面/体。
 *   - 返回 render id（局部 cell/point id），由各 selector 自行反查为模型 id。
 *
 * 与 demo 的差异（PreCess-specific）：
 *   - 加 PickingOpacityGuard：透明模式下临时把 target + keepVisible 的 opacity 置 1.0，
 *     避免半透明 actor 因深度写入关闭导致 picker 误命中背面元素。
 *   - 函数内不修改 actor 的 visibility 状态以外的内容，调用方负责 RAII 围一圈。
 */
#ifndef MESH_AREA_PICK_H
#define MESH_AREA_PICK_H

#include <algorithm>
#include <set>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>
#include <vtkActor.h>
#include <vtkActor2D.h>
#include <vtkDataObject.h>
#include <vtkHardwareSelector.h>
#include <vtkIdTypeArray.h>
#include <vtkInformation.h>
#include <vtkMapper.h>
#include <vtkMatrix4x4.h>
#include <vtkPolyData.h>
#include <vtkProp.h>
#include <vtkPropCollection.h>
#include <vtkProperty.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkSelection.h>
#include <vtkSelectionNode.h>
#include <vtkSmartPointer.h>

namespace area_pick {

//! @brief 临时把目标 actor 与 keepVisible 列表内 actor 的 opacity 强制设为 1.0，
//!        出作用域自动还原。PreCess 的 Transparent25/50/75 模式下 actor 半透明，
//!        VTK 默认对半透明 actor 关闭深度写入，导致 picker 拾到背面元素。
//!        用 RAII 围一圈 executeAreaPick 即可修复。
class PickingOpacityGuard {
public:
    PickingOpacityGuard(vtkActor* target, const std::vector<vtkActor*>* keep_visible)
    {
        if (target)
            saveAndSet(target);
        if (keep_visible) {
            for (auto* a : *keep_visible)
                saveAndSet(a);
        }
    }
    ~PickingOpacityGuard()
    {
        for (auto& [actor, opacity] : saved_) {
            actor->GetProperty()->SetOpacity(opacity);
        }
    }

    PickingOpacityGuard(const PickingOpacityGuard&) = delete;
    PickingOpacityGuard& operator=(const PickingOpacityGuard&) = delete;

private:
    void saveAndSet(vtkActor* actor)
    {
        if (!actor)
            return;
        saved_.emplace_back(actor, actor->GetProperty()->GetOpacity());
        actor->GetProperty()->SetOpacity(1.0);
    }

    std::vector<std::pair<vtkActor*, double>> saved_;
};

//! @brief 在指定屏幕矩形内对 target actor 执行硬件拾取
//! @param renderer       渲染器
//! @param target_actor   拾取目标 actor（其它 actor 临时隐藏，仅 keep_visible 列表中的保持可见）
//! @param xmin ymin xmax ymax  屏幕像素坐标矩形（一般由 interactor 给出）
//! @param field_association vtkDataObject::FIELD_ASSOCIATION_POINTS / _CELLS
//! @param keep_visible   可选：保持可见用于填充 z-buffer 的 actor 列表（POINTS/EDGES 模式填 face/solid）
//! @return 命中单元 id 集合（render id，与 target_actor 的 cell/point id 空间一致）
inline std::set<vtkIdType> executeAreaPick(
    vtkRenderer* renderer,
    vtkActor* target_actor,
    int xmin, int ymin, int xmax, int ymax,
    int field_association,
    const std::vector<vtkActor*>* keep_visible = nullptr)
{
    std::set<vtkIdType> result;
    if (!renderer || !target_actor)
        return result;

    spdlog::debug("[AreaPick] target={} rect=({},{},{},{}) fieldAssoc={} keepVis={} target.vis={} target.repr={}",
        target_actor->GetClassName(), xmin, ymin, xmax, ymax,
        field_association, keep_visible ? keep_visible->size() : 0,
        target_actor->GetVisibility(),
        target_actor->GetProperty()->GetRepresentationAsString());

    // 诊断 target actor 的 mapper 数据：pick 行为依赖 mapper 实际写 cell/point id 到 FBO
    if (auto* mapper = target_actor->GetMapper()) {
        if (auto* poly = vtkPolyData::SafeDownCast(mapper->GetInput())) {
            spdlog::debug("[AreaPick]   target.mapper input: points={} verts={} lines={} polys={} strips={}",
                poly->GetNumberOfPoints(), poly->GetNumberOfVerts(),
                poly->GetNumberOfLines(), poly->GetNumberOfPolys(),
                poly->GetNumberOfStrips());
        } else {
            spdlog::debug("[AreaPick]   target.mapper input is not vtkPolyData");
        }
    }

    // 1) 临时隔离：除 target + keepVisible 外的 actor 全部 VisibilityOff；
    //    vtkActor2D（如橡皮筋）也临时 VisibilityOff——picker.FBO 不应拾 2D props
    vtkPropCollection* props = renderer->GetViewProps();
    std::vector<std::pair<vtkProp*, int>> saved_visibility;
    std::vector<std::pair<vtkProp*, int>> saved_actor2d_vis;
    int saved_target_vis = target_actor->GetVisibility();
    target_actor->VisibilityOn();
    for (int i = 0; i < props->GetNumberOfItems(); ++i) {
        vtkObject* obj = props->GetItemAsObject(i);
        vtkProp* prop = vtkProp::SafeDownCast(obj);
        if (!prop || prop == target_actor)
            continue;
        // 2D props（如橡皮筋 actor2D）单独保存 visibility 后 VisibilityOff
        if (vtkActor2D::SafeDownCast(prop)) {
            saved_actor2d_vis.emplace_back(prop, prop->GetVisibility());
            prop->VisibilityOff();
            continue;
        }
        auto* actor = vtkActor::SafeDownCast(prop);
        if (!actor)
            continue;
        if (keep_visible
            && std::find(keep_visible->begin(), keep_visible->end(), actor) != keep_visible->end())
            continue; // keepVisible：用于填充 z-buffer，保持可见
        saved_visibility.emplace_back(actor, actor->GetVisibility());
        actor->VisibilityOff();
    }

    // 2) HardwareSelector 拾取
    vtkNew<vtkHardwareSelector> shared_selector;
    shared_selector->SetRenderer(renderer);
    shared_selector->SetArea(xmin, ymin, xmax, ymax);
    shared_selector->SetFieldAssociation(field_association);
    vtkSelection* sel = shared_selector->Select();

    // 3) 恢复 visibility
    target_actor->SetVisibility(saved_target_vis);
    for (auto& [prop, vis] : saved_visibility) {
        prop->SetVisibility(vis);
    }
    for (auto& [prop, vis] : saved_actor2d_vis) {
        prop->SetVisibility(vis);
    }

    if (!sel) {
        spdlog::warn("[AreaPick] shared_selector->Select() returned null");
        return result;
    }

    spdlog::debug("[AreaPick] sel nodes={} props_in_renderer={}",
        sel->GetNumberOfNodes(),
        renderer->GetViewProps()->GetNumberOfItems());

    // 4) 按 PROP 过滤：只保留 targetActor 产生的节点；keepVisible actor 的元素被剔除
    for (unsigned int i = 0; i < sel->GetNumberOfNodes(); ++i) {
        vtkSelectionNode* node = sel->GetNode(i);
        if (!node)
            continue;
        vtkProp* node_prop = vtkProp::SafeDownCast(
            node->GetProperties()->Get(vtkSelectionNode::PROP()));
        auto* list = vtkIdTypeArray::SafeDownCast(node->GetSelectionList());
        const bool matches_target = (node_prop == target_actor);
        spdlog::debug("[AreaPick]   node[{}]: prop={} matchesTarget={} listSize={}",
            i,
            node_prop ? node_prop->GetClassName() : "(null)",
            matches_target,
            list ? list->GetNumberOfTuples() : 0);
        if (node_prop && !matches_target)
            continue;
        if (!list)
            continue;
        for (vtkIdType j = 0; j < list->GetNumberOfTuples(); ++j) {
            vtkIdType id = list->GetValue(j);
            if (id >= 0)
                result.insert(id);
        }
    }
    spdlog::debug("[AreaPick] result.size()={}", result.size());

    return result;
}

//! @brief 把 executeAreaPick 与 PickingOpacityGuard 串成一次性调用，便于 selector 复用
inline std::set<vtkIdType> executeAreaPickWithGuard(
    vtkRenderer* renderer,
    vtkActor* target_actor,
    int xmin, int ymin, int xmax, int ymax,
    int field_association,
    const std::vector<vtkActor*>* keep_visible = nullptr)
{
    PickingOpacityGuard guard(target_actor, keep_visible);
    return executeAreaPick(renderer, target_actor,
        xmin, ymin, xmax, ymax, field_association, keep_visible);
}

//! @brief 数据空间 (lx,ly,lz) → world → display，测试屏幕投影是否落在框选矩形内。
//!        用于 face CELLS 拾取后的二次过滤，避免"面只一角进框、所有点/边都高亮"。
inline bool isWorldPointInScreenBox(vtkRenderer* renderer,
    vtkActor* transform_actor,
    double lx, double ly, double lz,
    int xmin, int ymin, int xmax, int ymax)
{
    if (!renderer)
        return false;
    double world[4] = { lx, ly, lz, 1.0 };
    vtkMatrix4x4* m = transform_actor ? transform_actor->GetMatrix() : nullptr;
    if (m)
        m->MultiplyPoint(world, world);
    renderer->SetWorldPoint(world);
    renderer->WorldToDisplay();
    const double* dp = renderer->GetDisplayPoint();
    return dp[0] >= xmin && dp[0] <= xmax
        && dp[1] >= ymin && dp[1] <= ymax;
}

//! @brief 线段（world→display 投影后）是否与框选矩形相交。
//!        任一端点在框内、或线段穿过矩形（含擦边）均视为相交（"与边相交即选中"语义）。
inline bool isScreenSegmentIntersectsBox(vtkRenderer* renderer,
    vtkActor* transform_actor,
    double lx0, double ly0, double lz0,
    double lx1, double ly1, double lz1,
    int xmin, int ymin, int xmax, int ymax)
{
    if (!renderer)
        return false;

    // 端点投影路径与 isWorldPointInScreenBox 完全一致
    auto project = [&](double lx, double ly, double lz) {
        double world[4] = { lx, ly, lz, 1.0 };
        vtkMatrix4x4* m = transform_actor ? transform_actor->GetMatrix() : nullptr;
        if (m)
            m->MultiplyPoint(world, world);
        renderer->SetWorldPoint(world);
        renderer->WorldToDisplay();
        const double* dp = renderer->GetDisplayPoint();
        return std::pair<double, double> { dp[0], dp[1] };
    };

    const auto p0 = project(lx0, ly0, lz0);
    const auto p1 = project(lx1, ly1, lz1);

    auto in_box = [&](double x, double y) {
        return x >= xmin && x <= xmax && y >= ymin && y <= ymax;
    };
    if (in_box(p0.first, p0.second) || in_box(p1.first, p1.second))
        return true;

    // Liang–Barsky：2D 线段 vs 轴对齐矩形（无端点落入时判是否穿过）
    const double dx = p1.first - p0.first;
    const double dy = p1.second - p0.second;
    const double p[4] = { -dx, dx, -dy, dy };
    const double q[4] = { p0.first - xmin, xmax - p0.first,
        p0.second - ymin, ymax - p0.second };
    double t0 = 0.0, t1 = 1.0;
    for (int i = 0; i < 4; ++i) {
        if (p[i] == 0.0) {
            if (q[i] < 0.0)
                return false; // 平行于该轴且在矩形外
        } else {
            const double r = q[i] / p[i];
            if (p[i] < 0.0) {
                if (r > t1)
                    return false;
                if (r > t0)
                    t0 = r;
            } else {
                if (r < t0)
                    return false;
                if (r < t1)
                    t1 = r;
            }
        }
    }
    return true;
}

} // namespace area_pick

#endif // MESH_AREA_PICK_H