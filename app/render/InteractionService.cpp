/**
 * @file InteractionService.cpp
 * @brief 通用视口交互服务实现：拾取解析、标注绘制、事件路由
 */

#include "InteractionService.h"

#include "CoincidentTopology.h"
#include "MeshActorManagerSelectOp.h"
#include "SelectManager.h"
#include "InteractionState.h"

#include <vtkActor.h>
#include <vtkBillboardTextActor3D.h>
#include <vtkCellArray.h>
#include <vtkDataSet.h>
#include <vtkHardwarePicker.h>
#include <vtkIdTypeArray.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkCellData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkTextProperty.h>
#include <vtkUnsignedCharArray.h>

namespace {

using systems::interaction::AnnotationBatch;
using systems::interaction::PickInfo;

//! @brief 创建统一风格的 3D 面向相机文字标注（置顶层内不被模型遮挡）
vtkSmartPointer<vtkBillboardTextActor3D> makeTextActor(vtkRenderer* renderer)
{
    auto text = vtkSmartPointer<vtkBillboardTextActor3D>::New();
    text->SetInput("");
    vtkTextProperty* prop = text->GetTextProperty();
    prop->SetFontSize(16);
    prop->SetBold(true);
    prop->SetJustificationToCentered();
    prop->SetVerticalJustificationToCentered();
    prop->SetBackgroundColor(0.0, 0.0, 0.0);
    prop->SetBackgroundOpacity(0.5);
    text->PickableOff();
    text->SetVisibility(false);
    renderer->AddActor(text);
    return text;
}

//! @brief 创建空颜色的标量数组
vtkSmartPointer<vtkUnsignedCharArray> makeColorArray(const char* name)
{
    auto colors = vtkSmartPointer<vtkUnsignedCharArray>::New();
    colors->SetName(name);
    colors->SetNumberOfComponents(3);
    return colors;
}

void appendColor(vtkUnsignedCharArray* colors, double r, double g, double b)
{
    const unsigned char rgb[3] = { static_cast<unsigned char>(r * 255.0),
        static_cast<unsigned char>(g * 255.0), static_cast<unsigned char>(b * 255.0) };
    colors->InsertNextTypedTuple(rgb);
}

} // namespace

InteractionService::InteractionService(vtkRenderer& renderer, vtkRenderer& overlay_renderer,
    MeshActorManagerSelectOp& mesh_op, SelectManager& select_manager)
    : renderer_(&renderer)
    , overlay_renderer_(&overlay_renderer)
    , mesh_op_(&mesh_op)
    , select_manager_(&select_manager)
{
    // 网格顶点拾取器：复用选择系统的 pick list，吸附网格顶点
    mesh_picker_ = vtkSmartPointer<vtkHardwarePicker>::New();
    mesh_picker_->SnapToMeshPointOn();
    mesh_picker_->SetPixelTolerance(5);
    mesh_picker_->PickFromListOn();
    mesh_op_->observePickList(mesh_picker_->GetPickList());

    // 标注点（按 DTO 逐点上色）
    points_poly_ = vtkSmartPointer<vtkPolyData>::New();
    vtkNew<vtkPolyDataMapper> points_mapper;
    points_mapper->SetInputData(points_poly_);
    points_mapper->SetRelativeCoincidentTopologyPointOffsetParameter(highlight::POINT_UNITS);
    points_mapper->SetScalarModeToUsePointFieldData();
    points_mapper->SelectColorArray("Colors");
    points_actor_ = vtkSmartPointer<vtkActor>::New();
    points_actor_->SetMapper(points_mapper);
    points_actor_->GetProperty()->SetPointSize(9.0);
    points_actor_->GetProperty()->SetRenderPointsAsSpheres(true);
    points_actor_->PickableOff();
    points_actor_->SetVisibility(false);
    renderer_->AddActor(points_actor_);

    // 实线标注（按 DTO 逐线上色）
    lines_poly_ = vtkSmartPointer<vtkPolyData>::New();
    vtkNew<vtkPolyDataMapper> lines_mapper;
    lines_mapper->SetInputData(lines_poly_);
    lines_mapper->SetRelativeCoincidentTopologyLineOffsetParameters(0, highlight::LINE_UNITS);
    lines_mapper->SetScalarModeToUseCellFieldData();
    lines_mapper->SelectColorArray("Colors");
    lines_actor_ = vtkSmartPointer<vtkActor>::New();
    lines_actor_->SetMapper(lines_mapper);
    lines_actor_->GetProperty()->SetLineWidth(2.0);
    lines_actor_->PickableOff();
    lines_actor_->SetVisibility(false);
    renderer_->AddActor(lines_actor_);

    // 虚线标注（悬停动态预览等）
    dashed_poly_ = vtkSmartPointer<vtkPolyData>::New();
    vtkNew<vtkPolyDataMapper> dashed_mapper;
    dashed_mapper->SetInputData(dashed_poly_);
    dashed_mapper->SetRelativeCoincidentTopologyLineOffsetParameters(0, highlight::LINE_UNITS);
    dashed_mapper->SetScalarModeToUseCellFieldData();
    dashed_mapper->SelectColorArray("Colors");
    dashed_actor_ = vtkSmartPointer<vtkActor>::New();
    dashed_actor_->SetMapper(dashed_mapper);
    dashed_actor_->GetProperty()->SetLineWidth(2.0);
    dashed_actor_->GetProperty()->SetLineStipplePattern(0xF0F0);
    dashed_actor_->GetProperty()->SetLineStippleRepeatFactor(1);
    dashed_actor_->PickableOff();
    dashed_actor_->SetVisibility(false);
    renderer_->AddActor(dashed_actor_);
}

InteractionService::~InteractionService()
{
    mesh_op_->unobservePickList(mesh_picker_->GetPickList());
    if (renderer_) {
        renderer_->RemoveActor(points_actor_);
        renderer_->RemoveActor(lines_actor_);
        renderer_->RemoveActor(dashed_actor_);
    }
    if (overlay_renderer_) {
        for (auto& t : text_pool_)
            overlay_renderer_->RemoveActor(t);
    }
}

void InteractionService::start(systems::interaction::InteractionState* state)
{
    if (state_ == state)
        return;
    if (state_)
        stop();
    state_ = state;
    if (!state_)
        return;

    // 交互时几何吸附切到顶点模式，停止时还原（经选择系统封装，不接触 picker）
    select_manager_->setVertexSnapActive(true);
    if (state_->on_activate)
        state_->on_activate();
    refreshAnnotations();
    emitResult();
}

void InteractionService::stop()
{
    if (!state_)
        return;
    if (state_->on_deactivate)
        state_->on_deactivate();
    state_->clearSession();
    state_ = nullptr;
    select_manager_->setVertexSnapActive(false);

    // 清空全部标注
    points_poly_->Initialize();
    lines_poly_->Initialize();
    dashed_poly_->Initialize();
    points_actor_->SetVisibility(false);
    lines_actor_->SetVisibility(false);
    dashed_actor_->SetVisibility(false);
    for (auto& t : text_pool_)
        t->SetVisibility(false);
    emitResult();
}

void InteractionService::pick(double posx, double posy)
{
    if (!state_)
        return;
    PickInfo info;
    if (!snapToPickInfo(posx, posy, info))
        return;
    if (state_->on_pick && state_->on_pick(info)) {
        refreshAnnotations();
        emitResult();
    }
}

void InteractionService::hover(double posx, double posy)
{
    if (!state_)
        return;
    PickInfo info;
    snapToPickInfo(posx, posy, info); // 未命中时 valid=false，由回调方清预览
    if (state_->on_hover && state_->on_hover(info)) {
        refreshAnnotations();
    }
}

void InteractionService::clear()
{
    if (!state_)
        return;
    if (state_->on_clear)
        state_->on_clear();
    refreshAnnotations();
    emitResult();
}

void InteractionService::emitResult()
{
    if (onResultChanged)
        onResultChanged(state_ ? state_->result_text : std::string());
}

bool InteractionService::snapToPickInfo(double posx, double posy, PickInfo& out)
{
    out = PickInfo {};

    // 优先吸附网格顶点
    mesh_picker_->Pick(posx, posy, 0, renderer_);
    const vtkIdType picked_point_id = mesh_picker_->GetPointId();
    if (picked_point_id != -1) {
        vtkDataSet* data_set = mesh_picker_->GetDataSet();
        if (!data_set)
            return false;
        // 位置取数据集存储的精确坐标：同一顶点跨次拾取位级一致，判同一点可靠
        double p[3];
        data_set->GetPoint(picked_point_id, p);
        out.world_pos = { p[0], p[1], p[2] };
        // 取全局顶点 id 作为附加判据
        if (auto ids = vtkIdTypeArray::SafeDownCast(
                data_set->GetPointData()->GetArray("vtkOriginalPointIds"))) {
            out.mesh_id = ids->GetValue(picked_point_id);
        }
        out.valid = true;
        return true;
    }

    // 未命中网格再尝试几何顶点（经选择系统封装接口，不接触 picker）
    if (auto hit = select_manager_->snapGeometryVertex(posx, posy)) {
        out.world_pos = hit->second;
        out.geom_id = hit->first;
        out.valid = true;
        return true;
    }
    return false;
}

void InteractionService::refreshAnnotations()
{
    const AnnotationBatch* batch = state_ ? &state_->annotations : nullptr;

    // 端点：全部标注点
    vtkNew<vtkPoints> pts;
    vtkNew<vtkCellArray> verts;
    auto pt_colors = makeColorArray("Colors");
    const std::size_t point_count = batch ? batch->points.size() : 0;
    pts->SetNumberOfPoints(static_cast<vtkIdType>(point_count));
    verts->SetNumberOfCells(static_cast<vtkIdType>(point_count));
    for (std::size_t i = 0; i < point_count; ++i) {
        const auto& ap = batch->points[i];
        pts->SetPoint(static_cast<vtkIdType>(i), ap.pos.data());
        verts->InsertCellPoint(static_cast<vtkIdType>(i));
        appendColor(pt_colors, ap.r, ap.g, ap.b);
    }
    points_poly_->SetPoints(pts);
    points_poly_->SetVerts(verts);
    points_poly_->GetPointData()->SetScalars(pt_colors);
    points_actor_->SetVisibility(point_count > 0);

    // 实线：非 dashed 标注线
    vtkNew<vtkPoints> line_pts;
    vtkNew<vtkCellArray> segs;
    auto line_colors = makeColorArray("Colors");
    if (batch) {
        for (const auto& al : batch->lines) {
            if (al.dashed)
                continue;
            const vtkIdType ids[2] = { line_pts->InsertNextPoint(al.p0.data()),
                line_pts->InsertNextPoint(al.p1.data()) };
            segs->InsertNextCell(2, ids);
            appendColor(line_colors, al.r, al.g, al.b);
        }
    }
    lines_poly_->SetPoints(line_pts);
    lines_poly_->SetLines(segs);
    lines_poly_->GetCellData()->SetScalars(line_colors);
    lines_actor_->SetVisibility(segs->GetNumberOfCells() > 0);

    // 虚线：dashed 标注线
    vtkNew<vtkPoints> dashed_pts;
    vtkNew<vtkCellArray> dashed_segs;
    auto dashed_colors = makeColorArray("Colors");
    if (batch) {
        for (const auto& al : batch->lines) {
            if (!al.dashed)
                continue;
            const vtkIdType ids[2] = { dashed_pts->InsertNextPoint(al.p0.data()),
                dashed_pts->InsertNextPoint(al.p1.data()) };
            dashed_segs->InsertNextCell(2, ids);
            appendColor(dashed_colors, al.r, al.g, al.b);
        }
    }
    dashed_poly_->SetPoints(dashed_pts);
    dashed_poly_->SetLines(dashed_segs);
    dashed_poly_->GetCellData()->SetScalars(dashed_colors);
    dashed_actor_->SetVisibility(dashed_segs->GetNumberOfCells() > 0);

    // 文本：池化复用 billboard actor，逐个上色
    std::size_t ti = 0;
    if (batch) {
        for (const auto& at : batch->texts) {
            while (text_pool_.size() <= ti)
                text_pool_.push_back(makeTextActor(overlay_renderer_));
            auto* t = text_pool_[ti].GetPointer();
            t->SetInput(at.text.c_str());
            t->SetPosition(at.pos[0], at.pos[1], at.pos[2]);
            t->GetTextProperty()->SetColor(at.r, at.g, at.b);
            t->SetVisibility(true);
            ++ti;
        }
    }
    for (; ti < text_pool_.size(); ++ti)
        text_pool_[ti]->SetVisibility(false);
}
