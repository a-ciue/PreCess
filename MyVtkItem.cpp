#include "MyVtkItem.h"

MyVtkItem::MyVtkItem()
{
    connect(this, &QQuickItem::widthChanged, this, &MyVtkItem::resetCamera);
    connect(this, &QQuickItem::heightChanged, this, &MyVtkItem::resetCamera);
}

QQuickVTKItem::vtkUserData MyVtkItem::initializeVTK(vtkRenderWindow* renderWindow)
{
    vtkNew<Data> vtk;

    vtk->actor->SetMapper(vtk->mapper);

    vtk->renderer->AddActor(vtk->actor);
    vtk->renderer->SetBackground(0.5, 0.5, 0.7);
    vtk->renderer->SetBackground2(0.7, 0.7, 0.7);
    vtk->renderer->SetGradientBackground(true);

    renderWindow->AddRenderer(vtk->renderer);

    // Remember: QML can delete our underlying QSGNode (which calls this method) at any time.
    // We have to re-synchronize our Qt properties with our VTK properties at any time.
    // To this end we've added a "force" parameter to our Qt property setter which is set true
    // here in initializeVtk but is defaulted false whenever QML (or other C++ code) invokes it.
    //
    // To see QML randomly delete our QSGNode, split horizontally, then split vertically and then
    // unsplit the smallest top-most view and observe the console output.
    //
    // You'll see something like:
    //
    // clang-format off
    //qml: constructed ItemDelegate(0x1f8d43946a0, "viewBase 0") SplitView_QMLTYPE_1_QML_5(0x1f8d4395660, "splitView 0")
    //qml: constructed ItemDelegate(0x1f8d4395780, "itemBase 0")
    //qml: constructed ItemDelegate(0x1f8d4395c00, "itemBase 1")
    //qml: constructed ItemDelegate(0x1f8d4394e80, "viewBase 1") SplitView_QMLTYPE_1_QML_5(0x1f8d4394fa0, "splitView 1")
    //qml: constructed ItemDelegate(0x1f8d43950c0, "itemBase 2")
    //qml: destructed ItemDelegate(0x1f8d4395780, "itemBase 0") SplitView_QMLTYPE_1_QML_5(0x1f8d4394fa0, "splitView 1")
    //qml: destructed ItemDelegate(0x1f8d4394e80, "viewBase 1") SplitView_QMLTYPE_1_QML_5(0x1f8d4395660, "splitView 0")
    // clang-format on
    //
    //    Notice that there are 2 (two) 'destructed' messages but you only unsplit once!!
    //    QML deleted both "small" QSGNodes and then created a new QSGNode to fill the empty column.
    dispatchChangedSource();

    // Note:  It is okay to store some non-graphical VTK objects in the QQuickVTKItem instead of the
    // vtkUserData but ONLY if they are accessed from the qml-render-thread. (i.e. only in the
    // initializeVTK, destroyingVTK or dispatch_async methods)
    vtk->renderer->GetActiveCamera()->DeepCopy(_camera);

    return vtk;
}

void MyVtkItem::destroyingVTK(vtkRenderWindow* renderWindow, vtkUserData userData)
{
    auto* vtk = Data::SafeDownCast(userData);
    _camera->DeepCopy(vtk->renderer->GetActiveCamera());
}

void MyVtkItem::resetCamera()
{
    dispatch_async([this](vtkRenderWindow* renderWindow, vtkUserData userData) {
        auto* vtk = Data::SafeDownCast(userData);
        vtk->renderer->ResetCamera();
        scheduleRender();
    });
}

void MyVtkItem::setSource(QString v)
{
    if (_source != v) {
        _source = v;
        dispatchChangedSource();
        emit sourceChanged(v);
    }
}

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
        else
            emit clicked();
        break;
    }
    default:
        break;
    }
    ev->accept();
    return true;
}

void MyVtkItem::dispatchChangedSource()
{
    dispatch_async([this](vtkRenderWindow* renderWindow, vtkUserData userData) {
        auto* vtk = Data::SafeDownCast(userData);
        // clang-format off
          vtk->mapper->SetInputConnection(
                _source == "Cone"    ? vtk->cone->GetOutputPort()
              : _source == "Sphere"  ? vtk->sphere->GetOutputPort()
              : _source == "Capsule" ? vtk->capsule->GetOutputPort()
              : (qWarning() << Q_FUNC_INFO << "YIKES!! Unknown source:'" << _source << "'", nullptr));
        // clang-format on

        resetCamera();
    });
}

void MyVtkItem::readFile(QUrl path) {
    dispatch_async([this, path](vtkRenderWindow* renderWindow, vtkUserData userData) {
        auto* vtk = Data::SafeDownCast(userData);
        vtkNew<vtkOBJReader> reader;
        reader->SetFileName(path.toLocalFile().toLatin1().data());
        reader->Update();
        vtk->mapper->SetInputConnection(reader->GetOutputPort());

        resetCamera();
    });
}

vtkStandardNewMacro(MyVtkItem::Data);
