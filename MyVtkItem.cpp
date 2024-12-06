#include "MyVtkItem.h"
#include "ModelUtil.h"
#include "ToolMesh.h"

MyVtkItem::MyVtkItem()
{
    connect(this, &QQuickItem::widthChanged, this, &MyVtkItem::resetCamera);
    connect(this, &QQuickItem::heightChanged, this, &MyVtkItem::resetCamera);
}

QQuickVTKItem::vtkUserData MyVtkItem::initializeVTK(vtkRenderWindow* renderWindow)
{
    vtkNew<Data> vtk;

    // Note:  It is okay to store some non-graphical VTK objects in the QQuickVTKItem instead of the
    // vtkUserData but ONLY if they are accessed from the qml-render-thread. (i.e. only in the
    // initializeVTK, destroyingVTK or dispatch_async methods)
    // vtk->renderer->GetActiveCamera()->DeepCopy(_camera);
    for (int i = 0; i < 3; i++) {
        vtk->renderer[i]->SetBackground(0.5, 0.5, 0.7);
        vtk->renderer[i]->SetBackground2(0.7, 0.7, 0.7);
        vtk->renderer[i]->SetGradientBackground(true);
    }
    vtk->blockStyle->SetSelector(std::make_unique<ActorSelectorHighlight>(vtk->renderer[1]));
    vtk->blockStyle->SetDefaultRenderer(vtk->renderer[1]);
    vtk->groupStyle->SetSelector(std::make_unique<ActorSelectorHighlight>(vtk->renderer[2]));
    vtk->groupStyle->SetDefaultRenderer(vtk->renderer[2]);
    vtk->faceStyle->SetSelector(std::make_unique<SingleFaceSelectorHighlight>(vtk->renderer[0]));
    vtk->faceStyle->SetDefaultRenderer(vtk->renderer[0]);
    vtk->edgeStyle->SetSelector(std::make_unique<SingleEdgeSelectorHighlight>(vtk->renderer[0], vtk->model.get()));
    vtk->edgeStyle->SetDefaultRenderer(vtk->renderer[0]);

    return vtk;
}

void MyVtkItem::destroyingVTK(vtkRenderWindow* renderWindow, vtkUserData userData)
{
    auto* vtk = Data::SafeDownCast(userData);
    if (vtk->curRenderer) {
        _camera->DeepCopy(vtk->curRenderer->GetActiveCamera());
    }
}

void MyVtkItem::resetCamera()
{
    dispatch_async([this](vtkRenderWindow* renderWindow, vtkUserData userData) {
        auto* vtk = Data::SafeDownCast(userData);
        if (vtk->curRenderer) {
            vtk->curRenderer->ResetCamera();
        }
        scheduleRender();
    });
}

// void MyVtkItem::setSource(QString v)
//{
//     if (_source != v) {
//         _source = v;
//         dispatchChangedSource();
//         emit sourceChanged(v);
//     }
// }

bool MyVtkItem::event(QEvent* ev)
{
    switch (ev->type()) {
    case QEvent::MouseButtonPress: {
        auto e = static_cast<QMouseEvent*>(ev);
        _click.reset(e->clone());
        break;
    }
    case QEvent::MouseMove: {
        if (!_click)
            return QQuickVTKItem::event(ev);

        auto e = static_cast<QMouseEvent*>(ev);
        if ((_click->position() - e->position()).manhattanLength() > 5) {
            QQuickVTKItem::event(QScopedPointer<QMouseEvent>(_click.take()).get());
            return QQuickVTKItem::event(e);
        }
        break;
    }
    case QEvent::MouseButtonRelease: {
        if (!_click)
            return QQuickVTKItem::event(ev);
        else {
            auto e = static_cast<QMouseEvent*>(ev);
            emit clicked(e->position().x(), e->position().y());
        }
        break;
    }
    default:
        break;
    }
    ev->accept();
    return true;
}

// void MyVtkItem::dispatchChangedSource()
//{
//     dispatch_async([this](vtkRenderWindow* renderWindow, vtkUserData userData) {
//         auto* vtk = Data::SafeDownCast(userData);
//         // clang-format off
//           vtk->mapper->SetInputConnection(
//                 _source == "Cone"    ? vtk->cone->GetOutputPort()
//               : _source == "Sphere"  ? vtk->sphere->GetOutputPort()
//               : _source == "Capsule" ? vtk->capsule->GetOutputPort()
//               : (qWarning() << Q_FUNC_INFO << "YIKES!! Unknown source:'" << _source << "'", nullptr));
//         // clang-format on
//
//         resetCamera();
//     });
// }

void MyVtkItem::readSpline(QUrl spline_path)
{
    dispatch_async([this, spline_path](vtkRenderWindow* renderWindow, vtkUserData userData) {
        auto* vtk = Data::SafeDownCast(userData);
        auto&& mesh = ModelUtil::mesh_from_spline(spline_path.toLocalFile().toLatin1().data());

        if (!mesh)
            emit splineLoadFailed(tr("fail to load spline file."));

        vtk->model = std::make_unique<Model>(std::move(mesh));

        vtk->model->actor().bind_renderer(vtk->renderer[0], ModelActor::RenderMode::Face);
        vtk->model->actor().bind_renderer(vtk->renderer[1], ModelActor::RenderMode::Block);
        vtk->model->actor().bind_renderer(vtk->renderer[2], ModelActor::RenderMode::Group);

        resetCamera();
    });
}

void MyVtkItem::writeMesh(QUrl spline_path)
{
}

void MyVtkItem::changeRenderer(QString renderMode)
{
    dispatch_async([this, renderMode](vtkRenderWindow* renderWindow, vtkUserData userData) {
        Data* vtk = Data::SafeDownCast(userData);

        if (vtk->curRenderer) {
            renderWindow->RemoveRenderer(vtk->curRenderer);
        }

        if (renderMode == "Face") {
            vtk->curRenderer = vtk->renderer[0];
        } else if (renderMode == "Block") {
            vtk->curRenderer = vtk->renderer[1];
        } else if (renderMode == "Group") {
            vtk->curRenderer = vtk->renderer[2];
        } else {
            std::cerr << "invalid renderMode in MyVtkItem::changeRenderer" << std::endl;
            return;
        }

        renderWindow->AddRenderer(vtk->curRenderer);

        resetCamera();
    });
}

void MyVtkItem::bindStyle(QString function)
{
    dispatch_async([this](vtkRenderWindow* renderWindow, vtkUserData userData) {
        Data* vtk = Data::SafeDownCast(userData);

		renderWindow->GetInteractor()->SetInteractorStyle(vtk->blockStyle);

        resetCamera();
    });
}

void MyVtkItem::unbindStyle(QString function)
{
}

void MyVtkItem::commitChange(QString function)
{
}

vtkStandardNewMacro(MyVtkItem::Data);
