// SPDX-License-Identifier: AGPL-3.0-or-later
/**
 * @file MeshAreaPick.h
 * @brief 网格框选的拾取与隔离工具
 *
 * vtkHardwareSelector 没有 pick list，会拾取当前可见的全部 actor。为只拿目标 actor
 * 自身的单元，拾取时临时把其它 actor VisibilityOff（keepVisible 列表内的 actor 保留，
 * 用于填充 z-buffer，避免点/边拾取穿透到背后的面/体），再按返回节点 PROP 过滤，
 * 返回目标 actor 的 render id（局部 cell/point id），由各 selector 自行反查为模型 id。
 *
 * PreCess 的 Transparent* 渲染模式会让 actor 半透明并关闭深度写入，拾取易误命中背面
 * 元素；拾取期间把 target/keepVisible 的 opacity 临时置 1.0 规避
 * （RAII 实现见 MeshAreaPick.cpp 内 PickingOpacityGuard）。
 */
#ifndef MESH_AREA_PICK_H
#define MESH_AREA_PICK_H

#include <set>
#include <vector>
#include <vtkType.h>

class vtkRenderer;
class vtkActor;

namespace area_pick {

//! @brief 在指定屏幕矩形内对 target actor 执行硬件拾取（含透明模式的 opacity guard）
//! @param renderer       渲染器
//! @param target_actor   拾取目标 actor（其它 actor 临时隐藏，仅 keep_visible 中的保持可见）
//! @param xmin ymin xmax ymax  屏幕像素坐标矩形（一般由 interactor 给出）
//! @param field_association vtkDataObject::FIELD_ASSOCIATION_POINTS / _CELLS
//! @param keep_visible   可选：保持可见用于填充 z-buffer 的 actor 列表（点/边拾取填面/体）
//! @return 命中单元 id 集合（render id，与 target_actor 的 cell/point id 空间一致）
std::set<vtkIdType> executeAreaPickWithGuard(
    vtkRenderer* renderer,
    vtkActor* target_actor,
    int xmin, int ymin, int xmax, int ymax,
    int field_association,
    const std::vector<vtkActor*>* keep_visible = nullptr);

//! @brief 数据空间 (lx,ly,lz) → world → display，测试屏幕投影是否落在框选矩形内。
//!        用于 face CELLS 拾取后的二次过滤，避免"面只一角进框、所有点/边都高亮"。
bool isWorldPointInScreenBox(vtkRenderer* renderer,
    vtkActor* transform_actor,
    double lx, double ly, double lz,
    int xmin, int ymin, int xmax, int ymax);

//! @brief 线段（world→display 投影后）是否与框选矩形相交。
//!        任一端点在框内、或线段穿过矩形（含擦边）均视为相交（"与边相交即选中"语义）。
bool isScreenSegmentIntersectsBox(vtkRenderer* renderer,
    vtkActor* transform_actor,
    double lx0, double ly0, double lz0,
    double lx1, double ly1, double lz1,
    int xmin, int ymin, int xmax, int ymax);

} // namespace area_pick

#endif // MESH_AREA_PICK_H
