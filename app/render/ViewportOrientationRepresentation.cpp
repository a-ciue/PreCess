#include "ViewportOrientationRepresentation.h"

#include <QFontDatabase>
#include <QFontMetricsF>
#include <QImage>
#include <QPainter>
#include <vtkCellArray.h>
#include <vtkCoordinate.h>
#include <vtkFloatArray.h>
#include <vtkImageData.h>
#include <vtkMatrix4x4.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper2D.h>
#include <vtkPropCollection.h>
#include <vtkRenderer.h>
#include <vtkTexture.h>
#include <vtkTexturedActor2D.h>
#include <vtkTransform.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace {
constexpr double kCanvasSize = 120.0;
constexpr double kCenter = kCanvasSize / 2;
constexpr double kAxisLength = 38.0;
constexpr double kHandleRadius = 10.0;
constexpr double kHoverRadius = 54.0;
constexpr double kSpriteSize = 28.0;

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
    vtkNew<vtkTexture> texture;
    vtkNew<vtkPoints> points;
    vtkNew<vtkPolyData> quad;
    vtkNew<vtkPolyDataMapper2D> mapper;
    vtkNew<vtkTexturedActor2D> actor;
    std::array<Handle, 6> handles {};
    std::array<double, 9> last_rotation {};
    bool has_projection = false;
    bool image_dirty = true;
    QImage canvas;
    QImage atlas;
    QSize sprite_size;
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
    SetXAxisColor(0.91, 0.23, 0.32);
    SetYAxisColor(0.43, 0.66, 0.10);
    SetZAxisColor(0.16, 0.47, 0.85);
    // vtkImageMapper 每帧重新包装输入并上传纹理；固定绑定同一图像才能复用 GPU 存储。
    impl_->texture->SetInputData(impl_->image);
    impl_->texture->SetColorModeToDirectScalars();
    impl_->texture->RepeatOff();
    impl_->texture->InterpolateOff();
    impl_->points->SetNumberOfPoints(4);
    impl_->quad->SetPoints(impl_->points);
    vtkNew<vtkCellArray> cells;
    const vtkIdType ids[] = { 0, 1, 2, 3 };
    cells->InsertNextCell(4, ids);
    impl_->quad->SetPolys(cells);
    vtkNew<vtkFloatArray> uv;
    uv->SetNumberOfComponents(2);
    uv->InsertNextTuple2(0, 0);
    uv->InsertNextTuple2(1, 0);
    uv->InsertNextTuple2(1, 1);
    uv->InsertNextTuple2(0, 1);
    impl_->quad->GetPointData()->SetTCoords(uv);
    impl_->mapper->SetInputData(impl_->quad);
    impl_->actor->SetMapper(impl_->mapper);
    impl_->actor->SetTexture(impl_->texture);
    impl_->actor->GetPositionCoordinate()->SetCoordinateSystemToViewport();
    impl_->actor->SetPosition(0, 0);
    impl_->actor->PickableOff();
}

ViewportOrientationRepresentation::~ViewportOrientationRepresentation() = default;

void ViewportOrientationRepresentation::buildAtlas(int width, int height)
{
    const double scale_x = width / kCanvasSize;
    const double scale_y = height / kCanvasSize;
    impl_->sprite_size = QSize(static_cast<int>(std::ceil(kSpriteSize * scale_x)),
        static_cast<int>(std::ceil(kSpriteSize * scale_y)));
    const auto cell = impl_->sprite_size;
    impl_->atlas = QImage(cell.width() * 12, cell.height(), QImage::Format_ARGB32_Premultiplied);
    impl_->atlas.fill(Qt::transparent);
    QPainter painter(&impl_->atlas);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    QFont font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    font.setWeight(QFont::DemiBold);
    font.setHintingPreference(QFont::PreferFullHinting);
    for (int axis = 0; axis < 3; ++axis) {
        const auto* rgb = GetAxisColor(axis);
        const QColor color = QColor::fromRgbF(rgb[0], rgb[1], rgb[2]);
        for (int direction = 0; direction < 2; ++direction) {
            for (int hovered = 0; hovered < 2; ++hovered) {
                const int column = axis * 4 + direction * 2 + hovered;
                const QPointF center(column * cell.width() + cell.width() / 2.0, cell.height() / 2.0);
                const QRectF circle(center.x() - kHandleRadius * scale_x, center.y() - kHandleRadius * scale_y,
                    2 * kHandleRadius * scale_x, 2 * kHandleRadius * scale_y);
                painter.setBrush(direction == 0 || hovered ? color : QColor(16, 22, 30, 100));
                const QColor outline = hovered ? QColor(255, 255, 255) : (direction == 0 ? color.darker(125) : color);
                painter.setPen(QPen(outline, (hovered ? 1.5 : 1.25) * scale_y));
                painter.drawEllipse(circle);
                if (direction != 0 && !hovered)
                    continue;
                // 正轴不再为负号让出字号；负轴仅悬停时使用略小的完整标签。
                font.setPixelSize(std::max(1, static_cast<int>(std::lround((direction == 0 ? 12.0 : 10.0) * scale_y))));
                painter.setFont(font);
                const QString label = (direction == 0 ? QString() : QStringLiteral("−")) + QString("XYZ").at(axis);
                const auto bounds = QFontMetricsF(font).tightBoundingRect(label);
                const QPoint origin(static_cast<int>(std::lround(center.x() - bounds.center().x())),
                    static_cast<int>(std::lround(center.y() - bounds.center().y())));
                painter.setPen(QColor(0, 0, 0, 210));
                painter.drawText(origin + QPoint(0, std::max(1, static_cast<int>(std::lround(scale_y)))), label);
                painter.setPen(Qt::white);
                painter.drawText(origin, label);
            }
        }
    }
}

void ViewportOrientationRepresentation::updateProjection()
{
    std::array<double, 9> rotation;
    auto* matrix = Transform->GetMatrix();
    for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 3; ++column)
            rotation[3 * row + column] = matrix->GetElement(row, column);
    // 原 widget 每帧 Identity/Rotate，MTime 总在变；比较实际旋转以复用六个投影。
    // 相同旋转输入可精确命中；累积误差只会触发重算，不用容差吞掉实际视角变化。
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

    if (impl_->canvas.size() != QSize(size[0], size[1])) {
        impl_->canvas = QImage(size[0], size[1], QImage::Format_RGBA8888);
        // 仅尺寸/DPR 变化时生成原生分辨率字形；旋转帧复用，不缩放预烘焙 PNG。
        buildAtlas(size[0], size[1]);
    }
    auto& image = impl_->canvas;
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    const double scale_x = size[0] / kCanvasSize;
    const double scale_y = size[1] / kCanvasSize;
    const QPointF center(kCenter * scale_x, kCenter * scale_y);
    if (GetInteractionStateAsEnum() != InteractionStateType::Outside) {
        painter.setPen(QPen(QColor(255, 255, 255, 24), scale_y));
        painter.setBrush(QColor(22, 29, 40, 28));
        painter.drawEllipse(center, kHoverRadius * scale_x, kHoverRadius * scale_y);
    }
    for (const auto& handle : impl_->handles) {
        const bool positive = handle.direction == 0;
        const bool hovered = handle.axis == PickedAxis && handle.direction == PickedDir;
        if (positive) {
            const double* rgb = GetAxisColor(handle.axis);
            painter.setPen(QPen(QColor::fromRgbF(rgb[0], rgb[1], rgb[2]), 1.8 * scale_y, Qt::SolidLine, Qt::RoundCap));
            painter.drawLine(center, QPointF(handle.center.x() * scale_x, handle.center.y() * scale_y));
        }
        // 字形缓存以整数像素位置 1:1 拷贝，避免分数坐标重采样软化细笔画。
        const int column = handle.axis * 4 + handle.direction * 2 + static_cast<int>(hovered);
        const auto cell = impl_->sprite_size;
        painter.drawImage(QPoint(static_cast<int>(std::lround(handle.center.x() * scale_x - cell.width() / 2.0)),
                              static_cast<int>(std::lround(handle.center.y() * scale_y - cell.height() / 2.0))),
            impl_->atlas, QRect(column * cell.width(), 0, cell.width(), cell.height()));
    }
    painter.end();

    // QPainter 在渲染线程只写自持 QImage；VTK 图像和控件也均随场景图在此线程拆解。
    if (size[0] != impl_->last_width || size[1] != impl_->last_height) {
        impl_->image->SetDimensions(size[0], size[1], 1);
        impl_->image->AllocateScalars(VTK_UNSIGNED_CHAR, 4);
        impl_->points->SetPoint(0, 0, 0, 0);
        impl_->points->SetPoint(1, size[0], 0, 0);
        impl_->points->SetPoint(2, size[0], size[1], 0);
        impl_->points->SetPoint(3, 0, size[1], 0);
        impl_->points->Modified();
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
            const double radius = kHandleRadius + 3.0;
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
    const int* size = Renderer ? Renderer->GetSize() : nullptr;
    return size && size[0] > 0 && size[1] > 0 && !impl_->atlas.isNull()
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
