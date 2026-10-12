#include "ViewportOrientationRepresentation.h"

#include <QImage>
#include <QPainter>
#include <vtkActor2D.h>
#include <vtkCoordinate.h>
#include <vtkImageData.h>
#include <vtkImageMapper.h>
#include <vtkMatrix4x4.h>
#include <vtkObjectFactory.h>
#include <vtkPropCollection.h>
#include <vtkRenderer.h>
#include <vtkTransform.h>

#include <algorithm>
#include <array>
#include <cstring>

namespace {
constexpr double kCanvasSize = 120.0;
constexpr double kCenter = kCanvasSize / 2;
constexpr double kAxisLength = 38.0;
constexpr double kPositiveRadius = 10.0;
constexpr double kNegativeRadius = 8.0;
constexpr double kHoverRadius = 54.0;
constexpr double kSpriteSize = 28.0;
constexpr int kSpritePixels = 84;

struct Handle {
    int axis;
    int direction;
    QPointF center;
    double depth;
};

std::array<Handle, 6> makeHandles(vtkTransform* transform)
{
    std::array<Handle, 6> handles;
    for (int axis = 0; axis < 3; ++axis) {
        for (int direction = 0; direction < 2; ++direction) {
            double point[3] {};
            point[axis] = direction == 0 ? 1.0 : -1.0;
            transform->TransformPoint(point, point);
            handles[2 * axis + direction] = { axis, direction,
                { kCenter + kAxisLength * point[0], kCenter - kAxisLength * point[1] }, point[2] };
        }
    }
    // 绘制和拾取共用深度顺序，轴向正视时只能命中最前面的圆点。
    std::stable_sort(handles.begin(), handles.end(), [](const Handle& a, const Handle& b) {
        return a.depth < b.depth;
    });
    return handles;
}
}

struct ViewportOrientationRepresentation::Impl {
    vtkNew<vtkImageData> image;
    vtkNew<vtkImageMapper> mapper;
    vtkNew<vtkActor2D> actor;
    std::array<Handle, 6> handles {};
    std::array<double, 9> last_rotation {};
    bool has_projection = false;
    bool image_dirty = true;
    QImage canvas;
    QImage atlas { QStringLiteral(":/orientation/assets/orientation-handles.png") };
    QImage hover { QStringLiteral(":/orientation/assets/orientation-hover.png") };
    int last_width {};
    int last_height {};
    int last_axis { -2 };
    int last_direction { -2 };
    int last_state { -1 };
};

vtkStandardNewMacro(ViewportOrientationRepresentation);

ViewportOrientationRepresentation::ViewportOrientationRepresentation()
    : impl_(std::make_unique<Impl>())
{
    SetSize(120, 120);
    SetPadding(12, 12);
    SetXAxisColor(0.96, 0.28, 0.38);
    SetYAxisColor(0.56, 0.78, 0.20);
    SetZAxisColor(0.24, 0.55, 0.95);
    impl_->mapper->SetInputData(impl_->image);
    impl_->mapper->SetColorWindow(255.0);
    impl_->mapper->SetColorLevel(127.5);
    impl_->actor->SetMapper(impl_->mapper);
    impl_->actor->GetPositionCoordinate()->SetCoordinateSystemToViewport();
    impl_->actor->SetPosition(0, 0);
    impl_->actor->PickableOff();
}

ViewportOrientationRepresentation::~ViewportOrientationRepresentation() = default;

void ViewportOrientationRepresentation::updateProjection()
{
    std::array<double, 9> rotation;
    auto* matrix = Transform->GetMatrix();
    for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 3; ++column)
            rotation[3 * row + column] = matrix->GetElement(row, column);
    // 原 widget 每帧 Identity/Rotate，MTime 总在变；比较实际旋转以复用六个投影。
    if (impl_->has_projection && rotation == impl_->last_rotation)
        return;
    impl_->handles = makeHandles(Transform);
    impl_->last_rotation = rotation;
    impl_->has_projection = true;
    impl_->image_dirty = true;
}

void ViewportOrientationRepresentation::BuildRepresentation()
{
    // 不调用父类：它会重建按钮几何和文字纹理；这里只保留其旋转/视角切换契约。
    if (!Renderer)
        return;
    const int* size = Renderer->GetSize();
    if (size[0] <= 0 || size[1] <= 0)
        return;
    updateProjection();
    if (!impl_->image_dirty && size[0] == impl_->last_width && size[1] == impl_->last_height
        && PickedAxis == impl_->last_axis && PickedDir == impl_->last_direction && InteractionState == impl_->last_state)
        return;

    if (impl_->canvas.size() != QSize(size[0], size[1]))
        impl_->canvas = QImage(size[0], size[1], QImage::Format_RGBA8888);
    auto& image = impl_->canvas;
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.scale(size[0] / kCanvasSize, size[1] / kCanvasSize);
    const QPointF center(kCenter, kCenter);
    if (GetInteractionStateAsEnum() != InteractionStateType::Outside)
        painter.drawImage(QRectF(0, 0, kCanvasSize, kCanvasSize), impl_->hover);
    for (const auto& handle : impl_->handles) {
        const bool positive = handle.direction == 0;
        const bool hovered = handle.axis == PickedAxis && handle.direction == PickedDir;
        if (positive) {
            const double* rgb = GetAxisColor(handle.axis);
            painter.setPen(QPen(QColor::fromRgbF(rgb[0], rgb[1], rgb[2]), 1.8, Qt::SolidLine, Qt::RoundCap));
            painter.drawLine(center, handle.center);
        }
        // 圆环、字母和高亮均来自离线 PNG 图集，旋转时只合成，不生成字形或圆环纹理。
        const int column = handle.axis * 4 + handle.direction * 2 + static_cast<int>(hovered);
        painter.drawImage(QRectF(handle.center.x() - kSpriteSize / 2, handle.center.y() - kSpriteSize / 2,
                              kSpriteSize, kSpriteSize),
            impl_->atlas, QRectF(column * kSpritePixels, 0, kSpritePixels, kSpritePixels));
    }
    painter.end();

    // QPainter 在渲染线程只写自持 QImage；VTK 图像和控件也均随场景图在此线程拆解。
    if (size[0] != impl_->last_width || size[1] != impl_->last_height) {
        impl_->image->SetDimensions(size[0], size[1], 1);
        impl_->image->AllocateScalars(VTK_UNSIGNED_CHAR, 4);
    }
    for (int row = 0; row < size[1]; ++row)
        std::memcpy(impl_->image->GetScalarPointer(0, row, 0), image.constScanLine(size[1] - row - 1), size[0] * 4);
    impl_->image->Modified();
    impl_->image_dirty = false;
    impl_->last_width = size[0];
    impl_->last_height = size[1];
    impl_->last_axis = PickedAxis;
    impl_->last_direction = PickedDir;
    impl_->last_state = InteractionState;
}

int ViewportOrientationRepresentation::ComputeInteractionState(int x, int y, int modify)
{
    const int old_axis = PickedAxis;
    const int old_direction = PickedDir;
    const int old_state = InteractionState;
    if (!Renderer || Renderer->GetSize()[0] <= 0 || Renderer->GetSize()[1] <= 0) {
        InteractionState = static_cast<int>(InteractionStateType::Outside);
        PickedAxis = PickedDir = -1;
        ValidPick = 0;
        return InteractionState;
    }
    const int* origin = Renderer->GetOrigin();
    const int* size = Renderer->GetSize();
    const QPointF point((x - origin[0]) * kCanvasSize / size[0],
        kCanvasSize - (y - origin[1]) * kCanvasSize / size[1]);
    if (modify) {
        // release 后 modify=1 先解除 Rotating，下一次点击才能正常切换视角。
        InteractionState = static_cast<int>(containsPoint(x, y) ? InteractionStateType::Hovering : InteractionStateType::Outside);
    }
    if (GetInteractionStateAsEnum() == InteractionStateType::Rotating)
        return InteractionState;
    PickedAxis = PickedDir = -1;
    ValidPick = 0;
    if (GetInteractionStateAsEnum() != InteractionStateType::Outside) {
        updateProjection();
        for (auto it = impl_->handles.rbegin(); it != impl_->handles.rend(); ++it) {
            const double radius = (it->direction == 0 ? kPositiveRadius : kNegativeRadius) + 3.0;
            const auto delta = point - it->center;
            if (delta.x() * delta.x() + delta.y() * delta.y() <= radius * radius) {
                PickedAxis = it->axis;
                PickedDir = it->direction;
                ValidPick = 1;
                break;
            }
        }
    }
    if (PickedAxis != old_axis || PickedDir != old_direction || InteractionState != old_state)
        Modified();
    return InteractionState;
}

int ViewportOrientationRepresentation::RenderOverlay(vtkViewport* viewport)
{
    BuildRepresentation();
    return Renderer && Renderer->GetSize()[0] > 0 && Renderer->GetSize()[1] > 0
        ? impl_->actor->RenderOverlay(viewport)
        : 0;
}

void ViewportOrientationRepresentation::ReleaseGraphicsResources(vtkWindow* window)
{
    impl_->actor->ReleaseGraphicsResources(window);
    Superclass::ReleaseGraphicsResources(window);
}

bool ViewportOrientationRepresentation::containsPoint(int x, int y) const
{
    if (!Renderer)
        return false;
    const int* origin = Renderer->GetOrigin();
    const int* size = Renderer->GetSize();
    if (size[0] <= 0 || size[1] <= 0)
        return false;
    const double dx = (x - origin[0]) * kCanvasSize / size[0] - kCenter;
    const double dy = (y - origin[1]) * kCanvasSize / size[1] - kCenter;
    return dx * dx + dy * dy <= kHoverRadius * kHoverRadius;
}

void ViewportOrientationRepresentation::GetActors2D(vtkPropCollection* props)
{
    if (props && GetVisibility())
        props->AddItem(impl_->actor);
}
