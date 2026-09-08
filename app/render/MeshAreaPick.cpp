// SPDX-License-Identifier: AGPL-3.0-or-later
/**
 * @file MeshAreaPick.cpp
 * @brief 网格框选的拾取与隔离实现（见 MeshAreaPick.h 的机制说明）
 */
#include "MeshAreaPick.h"

#include <spdlog/spdlog.h>
#include <vtkActor.h>
#include <vtkActor2D.h>
#include <vtkHardwarePicker.h>
#include <vtkHardwareSelector.h>
#include <vtkIdTypeArray.h>
#include <vtkInformation.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkProp.h>
#include <vtkPropCollection.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkSelection.h>
#include <vtkSelectionNode.h>

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace area_pick {
namespace {

//! @brief 临时把 actors 的 opacity 置 1.0，出作用域自动还原。
//!        规避 Transparent* 半透明 actor 关闭深度写入导致的背面误拾。
class PickingOpacityGuard {
public:
    explicit PickingOpacityGuard(const std::vector<vtkActor*>& actors)
    {
        for (auto* a : actors)
            saveAndSet(a);
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

} // namespace

std::unordered_map<vtkProp*, std::unordered_set<vtkIdType>> executeAreaPicks(
    vtkRenderer* renderer,
    const std::vector<vtkActor*>& target_actors,
    int xmin, int ymin, int xmax, int ymax,
    int field_association)
{
    std::unordered_map<vtkProp*, std::unordered_set<vtkIdType>> result;
    if (!renderer || target_actors.empty())
        return result;

    spdlog::debug("[AreaPick] multi targets={} rect=({},{},{},{}) fieldAssoc={}",
        target_actors.size(), xmin, ymin, xmax, ymax, field_association);

    // 透明模式下临时把所有 target 的 opacity 置 1.0（RAII 还原）
    PickingOpacityGuard opacity_guard(target_actors);

    // 记录并临时强制 target 可见：组件可见但其模式相关 actor 可能被 render style 关闭
    std::vector<std::pair<vtkActor*, int>> saved_target_vis;
    saved_target_vis.reserve(target_actors.size());
    for (auto* a : target_actors) {
        saved_target_vis.emplace_back(a, a->GetVisibility());
        a->VisibilityOn();
    }

    // 1) 临时隔离：除 target 外的 actor 全部 VisibilityOff；
    //    vtkActor2D（如橡皮筋）也临时 VisibilityOff——picker.FBO 不应拾 2D props
    std::vector<std::pair<vtkProp*, int>> saved_visibility;
    std::vector<std::pair<vtkProp*, int>> saved_actor2d_vis;
    vtkPropCollection* props = renderer->GetViewProps();
    for (int i = 0; i < props->GetNumberOfItems(); ++i) {
        vtkProp* prop = vtkProp::SafeDownCast(props->GetItemAsObject(i));
        if (!prop)
            continue;
        if (std::find(target_actors.begin(), target_actors.end(), prop) != target_actors.end())
            continue; // target：保持可见（互为 z-buffer）
        if (vtkActor2D::SafeDownCast(prop)) {
            saved_actor2d_vis.emplace_back(prop, prop->GetVisibility());
            prop->VisibilityOff();
            continue;
        }
        auto* actor = vtkActor::SafeDownCast(prop);
        if (!actor)
            continue;
        saved_visibility.emplace_back(actor, actor->GetVisibility());
        actor->VisibilityOff();
    }

    // 2) 预热：新组件 GPU 资源首次 Select() 时未就绪，需同步渲染一次。
    //    用 Pick() 而非 Render()——后者是调度式的，从交互线程调用异步空转。
    vtkNew<vtkHardwarePicker> prewarm;
    prewarm->Pick((xmin + xmax) / 2, (ymin + ymax) / 2, 0, renderer);

    // 3) HardwareSelector 一次拾取
    vtkNew<vtkHardwareSelector> shared_selector;
    shared_selector->SetRenderer(renderer);
    shared_selector->SetArea(xmin, ymin, xmax, ymax);
    shared_selector->SetFieldAssociation(field_association);
    vtkSelection* sel = shared_selector->Select();

    // 4) 恢复 visibility
    for (auto& [actor, vis] : saved_target_vis) {
        actor->SetVisibility(vis);
    }
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

    // 5) 按 PROP 分组；只保留 target_actors 产生的节点（隐藏组件未渲染 → 无节点）
    std::unordered_set<vtkProp*> target_set(target_actors.begin(), target_actors.end());
    for (unsigned int i = 0; i < sel->GetNumberOfNodes(); ++i) {
        vtkSelectionNode* node = sel->GetNode(i);
        if (!node)
            continue;
        vtkProp* node_prop = vtkProp::SafeDownCast(
            node->GetProperties()->Get(vtkSelectionNode::PROP()));
        if (!node_prop || !target_set.count(node_prop))
            continue;
        auto* list = vtkIdTypeArray::SafeDownCast(node->GetSelectionList());
        if (!list)
            continue;
        auto& ids = result[node_prop];
        for (vtkIdType j = 0; j < list->GetNumberOfTuples(); ++j) {
            vtkIdType id = list->GetValue(j);
            if (id >= 0)
                ids.insert(id);
        }
    }
    spdlog::debug("[AreaPick] hit actor count={}", result.size());

    return result;
}

bool isWorldPointInScreenBox(vtkRenderer* renderer,
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

bool isScreenSegmentIntersectsBox(vtkRenderer* renderer,
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
