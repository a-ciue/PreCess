#ifndef FACE_HIDE_FILTER_H
#define FACE_HIDE_FILTER_H

/**
 * @file FaceHideFilter.h
 * @brief 类型保真的 vtkPolyData 整单元移除过滤器（右键隐藏面功能使用）
 *
 * vtkRemovePolyData 经 vtkArrayListTemplate 复制单元属性时默认 promote=true，
 * 会把整型数组（含拾取身份数组 PrecessFaceIds，vtkIdTypeArray）提升为
 * vtkFloatArray，导致下游 vtkIdTypeArray::SafeDownCast 失败、拾取 id 映射错乱。
 * 本过滤器改用 vtkDataSetAttributes::CopyAllocate(COPYTUPLE)+CopyData 复制
 * 属性，输出保持每个数组的原始数据类型；点做压缩重排，点/单元属性随拷贝透传。
 */

#include <vtkIdTypeArray.h>
#include <vtkPolyDataAlgorithm.h>

class FaceHideFilter : public vtkPolyDataAlgorithm {
public:
    static FaceHideFilter* New();
    vtkTypeMacro(FaceHideFilter, vtkPolyDataAlgorithm);
    void PrintSelf(ostream& os, vtkIndent indent) override;

    //! @brief 待移除单元的 id 数组（输入 vtkPolyData 的全局单元 id，无需排序）；nullptr/空数组表示不移除
    void SetHiddenCellIds(vtkIdTypeArray* ids);
    vtkGetObjectMacro(HiddenCellIds, vtkIdTypeArray);

protected:
    FaceHideFilter();
    ~FaceHideFilter() override;

    int RequestData(vtkInformation*, vtkInformationVector**, vtkInformationVector*) override;

private:
    FaceHideFilter(const FaceHideFilter&) = delete;
    void operator=(const FaceHideFilter&) = delete;

    vtkIdTypeArray* HiddenCellIds {};
};

#endif // FACE_HIDE_FILTER_H
