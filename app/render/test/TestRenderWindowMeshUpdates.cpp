/**
 * @file TestRenderWindowMeshUpdates.cpp
 * @brief 不显示窗口地验证网格更新与属性渲染取消之间的边界。
 */
#include "MakeMeshData.h"
#include "MakeMeshDataVtk.h"
#include "MeshActorManager.h"
#include "QRenderWindow.h"
#include "renderStrategy/AttributeOperator.h"

#include <QGuiApplication>
#include <QImage>
#include <QQuickRenderControl>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <catch2/catch_test_macros.hpp>
#include <vtkActor.h>
#include <vtkPolyDataMapper.h>
#include <vtkTexture.h>

namespace {
/** @brief 由渲染控制器同步真实 VTK 回调，整个过程不显示 Qt Quick 窗口。 */
struct MeshUpdateViewport {
    QQuickRenderControl control;
    QQuickWindow window { &control };
    QRenderWindow item;

    /** @brief 初始化离屏场景，避免使用 show() 驱动渲染队列。 */
    MeshUpdateViewport()
    {
        window.resize(320, 240);
        item.setParentItem(window.contentItem());
        item.setSize(QSizeF(320, 240));
        REQUIRE(control.initialize());
    }

    /** @brief 在控件析构前释放场景图及其 VTK 资源。 */
    ~MeshUpdateViewport() { control.invalidate(); }

    /** @brief 消费已排队的网格安装及着色，并确认窗口始终不可见。 */
    void flush()
    {
        bool completed = false;
        item.dispatch_async([&completed](vtkRenderWindow*, QQuickVTKItem::vtkUserData) { completed = true; });
        control.polishItems();
        control.beginFrame();
        control.sync();
        control.endFrame();
        REQUIRE(completed);
        REQUIRE_FALSE(window.isVisible());
    }
};
}

TEST_CASE("Mesh reload preserves non-scalar rendering and explicit scalar cancellation still works", "[scalar-range][no-window]")
{
#ifdef Q_OS_WIN
    // Windows 的 offscreen 插件不提供 OpenGL；渲染控制器使用原生上下文但不显示窗口。
    qputenv("QT_QPA_PLATFORM", "windows");
#else
    qputenv("QT_QPA_PLATFORM", "offscreen");
#endif
    QQuickVTKItem::setGraphicsApi();
    int argc = 1;
    char app_name[] = "TestRenderWindowMeshUpdates";
    char* argv[] = { app_name, nullptr };
    QGuiApplication app(argc, argv);
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const auto texture_path = directory.filePath("texture.png");
    QImage image(2, 2, QImage::Format_RGB32);
    image.fill(Qt::white);
    REQUIRE(image.save(texture_path));

    auto mesh = MakeMeshDataWithAtri();
    MeshUpdateViewport viewport;
    viewport.item.loadMeshData(7, MakeMeshDataVtk(mesh));
    QRenderWindow::Data* scene = nullptr;
    viewport.item.dispatch_async([&scene](vtkRenderWindow*, QQuickVTKItem::vtkUserData user_data) {
        scene = QRenderWindow::Data::SafeDownCast(user_data);
    });
    viewport.flush();
    REQUIRE(scene);
    const auto actor = scene->mesh_actor_manager_->getComponentActor(7);
    REQUIRE(actor);
    AttributeOperator attributes(actor.get());

    // 普通网格重载不应额外执行通用取消：UV 贴图应保持绑定。
    REQUIRE(viewport.item.setAttriMode(7, "v_uv_2", static_cast<int>(Mode::UV), { { "texture_path", texture_path } }));
    viewport.flush();
    vtkSmartPointer<vtkTexture> texture = attributes.getFaceActor()->GetTexture();
    REQUIRE(texture);
    viewport.item.loadMeshData(7, MakeMeshDataVtk(mesh));
    viewport.flush();
    CHECK(attributes.getFaceActor()->GetTexture() == texture.GetPointer());

    // 向量策略同样不能因缺少标量配置而被取消。
    REQUIRE(viewport.item.setAttriMode(7, "f_normal_3", static_cast<int>(Mode::VECTOR), { }));
    viewport.flush();
    REQUIRE(attributes.getGlyph3DActor()->GetVisibility());
    viewport.item.loadMeshData(7, MakeMeshDataVtk(mesh));
    viewport.flush();
    CHECK(attributes.getGlyph3DActor()->GetVisibility());
    viewport.item.cancelComponentAttri(7);
    viewport.flush();
    CHECK_FALSE(attributes.getGlyph3DActor()->GetVisibility());

    // 无筛选时取消标量也必须关闭着色，不能只删除快照中的配置。
    REQUIRE(viewport.item.setAttriMode(7, "f_scalar_1", static_cast<int>(Mode::SCALAR), { }));
    viewport.flush();
    REQUIRE(attributes.getFaceMapper()->GetScalarVisibility());
    viewport.item.cancelComponentAttri(7);
    viewport.flush();
    CHECK_FALSE(attributes.getFaceMapper()->GetScalarVisibility());
}
