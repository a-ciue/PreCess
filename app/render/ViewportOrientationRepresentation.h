/**
 * @file ViewportOrientationRepresentation.h
 * @brief 圆点轴标：保留 VTK 视角切换与旋转交互，使用屏幕空间绘制。
 */
#ifndef VIEWPORT_ORIENTATION_REPRESENTATION_H
#define VIEWPORT_ORIENTATION_REPRESENTATION_H

#include <vtkCameraOrientationRepresentation.h>

#include <memory>

class ViewportOrientationRepresentation : public vtkCameraOrientationRepresentation {
public:
    static ViewportOrientationRepresentation* New();
    vtkTypeMacro(ViewportOrientationRepresentation, vtkCameraOrientationRepresentation);

    void BuildRepresentation() override;
    /** @brief 只读屏幕圆盘命中测试，不触发拾取、渲染或交互状态变更。 */
    bool containsPoint(int x, int y) const;
    void GetActors(vtkPropCollection*) override { }
    void GetActors2D(vtkPropCollection* props) override;
    int ComputeInteractionState(int x, int y, int modify = 0) override;
    int RenderOpaqueGeometry(vtkViewport*) override { return 0; }
    int RenderTranslucentPolygonalGeometry(vtkViewport*) override { return 0; }
    vtkTypeBool HasTranslucentPolygonalGeometry() override { return 0; }
    int RenderOverlay(vtkViewport* viewport) override;
    void ReleaseGraphicsResources(vtkWindow* window) override;

protected:
    ViewportOrientationRepresentation();
    ~ViewportOrientationRepresentation() override;
    // 不生成默认按钮纹理；圆点和字母由同一张抗锯齿图像绘制。
    void HighlightHandle() override { }

private:
    //! @brief 按实际渲染像素生成清晰的圆点和字形缓存，仅尺寸变化时调用。
    void buildAtlas(int width, int height);
    void updateProjection();
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

#endif
