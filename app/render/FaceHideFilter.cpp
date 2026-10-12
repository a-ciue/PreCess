#include "FaceHideFilter.h"

#include <spdlog/spdlog.h>
#include <vtkCellArray.h>
#include <vtkCellData.h>
#include <vtkDataSetAttributes.h>
#include <vtkIdList.h>
#include <vtkInformation.h>
#include <vtkInformationVector.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>

#include <vector>

vtkStandardNewMacro(FaceHideFilter);
vtkCxxSetObjectMacro(FaceHideFilter, HiddenCellIds, vtkIdTypeArray);

FaceHideFilter::FaceHideFilter() = default;

FaceHideFilter::~FaceHideFilter()
{
    this->SetHiddenCellIds(nullptr);
}

void FaceHideFilter::PrintSelf(ostream& os, vtkIndent indent)
{
    this->Superclass::PrintSelf(os, indent);
    os << indent << "HiddenCellIds: " << this->HiddenCellIds << "\n";
}

namespace {

//! @brief 复制一个分类单元数组（verts/lines/polys/strips）中未被移除的单元
//! @param in_cells 输入分类单元数组
//! @param removed 全局单元 id 移除标记
//! @param[in,out] cell_id 该分类数组起始的全局单元 id（按四类顺序递增），随遍历推进
//! @param[in,out] out_cell_id 输出全局单元 id，跨分类累计，仅在保留单元时递增
//! @param point_map 输入点 id → 输出点 id 压缩映射，-1 表示尚未复制
//! @param in_points/in_point_data/out_points/out_point_data 点与点属性复制上下文
//! @param in_cell_data/out_cell_data 单元属性复制上下文
//! @return 新建的单元数组（可能为空），调用方负责挂到输出 polydata
vtkNew<vtkCellArray> copyVisibleCells(vtkCellArray* in_cells,
    const std::vector<char>& removed,
    vtkIdType& cell_id,
    vtkIdType& out_cell_id,
    std::vector<vtkIdType>& point_map,
    vtkPoints* in_points,
    vtkPointData* in_point_data,
    vtkPoints* out_points,
    vtkPointData* out_point_data,
    vtkCellData* in_cell_data,
    vtkCellData* out_cell_data)
{
    vtkNew<vtkCellArray> out_cells;
    if (!in_cells || in_cells->GetNumberOfCells() == 0)
        return out_cells;

    const vtkIdType cell_count = in_cells->GetNumberOfCells();
    out_cells->AllocateEstimate(cell_count, 4);

    vtkNew<vtkIdList> point_ids;
    vtkNew<vtkIdList> new_point_ids;
    for (vtkIdType local_id = 0; local_id < cell_count; ++local_id, ++cell_id) {
        in_cells->GetCellAtId(local_id, point_ids);

        if (removed[static_cast<size_t>(cell_id)])
            continue;

        const vtkIdType point_id_count = point_ids->GetNumberOfIds();
        new_point_ids->SetNumberOfIds(point_id_count);
        for (vtkIdType j = 0; j < point_id_count; ++j) {
            const vtkIdType point_id = point_ids->GetId(j);
            vtkIdType new_point_id = point_map[static_cast<size_t>(point_id)];
            if (new_point_id == -1) {
                new_point_id = out_points->InsertNextPoint(in_points->GetPoint(point_id));
                out_point_data->CopyData(in_point_data, point_id, new_point_id);
                point_map[static_cast<size_t>(point_id)] = new_point_id;
            }
            new_point_ids->SetId(j, new_point_id);
        }

        out_cells->InsertNextCell(new_point_ids);
        // 分类数组的单元索引各自从零开始，属性必须按整个输出数据集的单元索引写入。
        out_cell_data->CopyData(in_cell_data, cell_id, out_cell_id);
        ++out_cell_id;
    }

    return out_cells;
}

} // namespace

int FaceHideFilter::RequestData(
    vtkInformation*,
    vtkInformationVector** input_vector,
    vtkInformationVector* output_vector)
{
    auto* input = vtkPolyData::GetData(input_vector[0]);
    auto* output = vtkPolyData::GetData(output_vector);
    if (!input || !output) {
        spdlog::error("FaceHideFilter::RequestData 输入或输出数据为空：输入有效={}，输出有效={}",
            input != nullptr, output != nullptr);
        return 0;
    }

    // 空隐藏集（含未设置）透传完整数据，但用新的 cell array wrapper 包裹底层数据，
    // 使每个 wrapper 拥有独立 MTime。这是修复 vtkOpenGLCellToVTKCellMap 缓存失效
    // 问题的关键：若直接 ShallowCopy 共享 cell array 对象，则 filter 输出的
    // cell array MTime 与原始 face_data_ 相同，当输入从"深拷贝(隐藏)"切回"透传(显示)"
    // 时 TempState 会回退到之前的值，与 MapBuildState 相等，导致 Update 跳过重建，
    // CellCellMap 留空，拾取时 .at() 越界崩溃。
    const vtkIdType hidden_count = this->HiddenCellIds ? this->HiddenCellIds->GetNumberOfValues() : 0;
    if (hidden_count == 0) {
        output->SetPoints(input->GetPoints());
        output->GetPointData()->ShallowCopy(input->GetPointData());
        output->GetCellData()->ShallowCopy(input->GetCellData());

        auto wrapCells = [](vtkCellArray* in_arr) -> vtkSmartPointer<vtkCellArray> {
            if (!in_arr)
                return nullptr;
            auto out_arr = vtkSmartPointer<vtkCellArray>::New();
            out_arr->ShallowCopy(in_arr);
            return out_arr;
        };
        output->SetVerts(wrapCells(input->GetVerts()));
        output->SetLines(wrapCells(input->GetLines()));
        output->SetPolys(wrapCells(input->GetPolys()));
        output->SetStrips(wrapCells(input->GetStrips()));
        return 1;
    }

    const vtkIdType cell_count = input->GetNumberOfCells();
    const vtkIdType point_count = input->GetNumberOfPoints();
    vtkPoints* in_points = input->GetPoints();
    if (!in_points && cell_count > 0) {
        spdlog::error("FaceHideFilter::RequestData 输入包含 {} 个单元，但缺少点坐标", cell_count);
        return 0;
    }

    // 标记待移除单元，越界 id 忽略。
    std::vector<char> removed(static_cast<size_t>(cell_count), 0);
    for (vtkIdType i = 0; i < hidden_count; ++i) {
        const vtkIdType id = this->HiddenCellIds->GetValue(i);
        if (id >= 0 && id < cell_count)
            removed[static_cast<size_t>(id)] = 1;
    }

    vtkNew<vtkPoints> out_points;
    if (in_points)
        out_points->SetDataType(in_points->GetDataType());
    output->SetPoints(out_points);

    // COPYTUPLE 只做整元组拷贝，输出数组按输入数组同类型创建（整型不提升为浮点）。
    vtkPointData* in_point_data = input->GetPointData();
    vtkPointData* out_point_data = output->GetPointData();
    out_point_data->CopyAllocate(in_point_data, point_count, 0, vtkDataSetAttributes::COPYTUPLE);

    vtkCellData* in_cell_data = input->GetCellData();
    vtkCellData* out_cell_data = output->GetCellData();
    out_cell_data->CopyAllocate(in_cell_data, cell_count, 0, vtkDataSetAttributes::COPYTUPLE);

    // 输入点 id → 输出点 id 的压缩映射（只保留被可见面引用的点）。
    std::vector<vtkIdType> point_map(static_cast<size_t>(point_count), -1);

    // 全局单元 id 顺序为 verts → lines → polys → strips（与 vtkRemovePolyData 文档一致）。
    vtkIdType cell_id = 0;
    vtkIdType out_cell_id = 0;
    auto out_verts = copyVisibleCells(input->GetVerts(), removed, cell_id, out_cell_id, point_map,
        in_points, in_point_data, out_points, out_point_data, in_cell_data, out_cell_data);
    auto out_lines = copyVisibleCells(input->GetLines(), removed, cell_id, out_cell_id, point_map,
        in_points, in_point_data, out_points, out_point_data, in_cell_data, out_cell_data);
    auto out_polys = copyVisibleCells(input->GetPolys(), removed, cell_id, out_cell_id, point_map,
        in_points, in_point_data, out_points, out_point_data, in_cell_data, out_cell_data);
    auto out_strips = copyVisibleCells(input->GetStrips(), removed, cell_id, out_cell_id, point_map,
        in_points, in_point_data, out_points, out_point_data, in_cell_data, out_cell_data);

    output->SetVerts(out_verts);
    output->SetLines(out_lines);
    output->SetPolys(out_polys);
    output->SetStrips(out_strips);

    return 1;
}
