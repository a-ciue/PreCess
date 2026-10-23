# QLogManager Qt/QML 消息桥接测试
# app 层无库目标（QLogManager 仅编入 PreCess 可执行目标，测试无法链接），
# 故直接编译源文件并启用 AUTOMOC
precess_add_test(TestQLogManager
    TestQLogManager.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../QLogManager.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../QLogManager.h"
)
set_target_properties(TestQLogManager PROPERTIES AUTOMOC ON)
target_include_directories(TestQLogManager PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
precess_test_link_libraries(TestQLogManager
    Qt${VTK_QT_VERSION}::Core
    Qt${VTK_QT_VERSION}::Qml
    spdlog::spdlog
)

# QPythonRuntime 主线程延时回调测试
# QPythonRuntime 与 precess_app 模块注册均编入 pythonApp 静态库（真/桩按可用性
# 二选一，宿主配置宏经 precess_runtime PUBLIC 传递），测试直接链接即可；Python
# 嵌入不可用（桩实现）时用例自行跳过
precess_add_test(TestQPythonRuntime TestQPythonRuntime.cpp)
target_include_directories(TestQPythonRuntime PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../model")
precess_test_link_libraries(TestQPythonRuntime
    Qt${VTK_QT_VERSION}::Core
    pythonApp
)

# JobProgressSlot 进度槽（纯 C++ 头文件，无 Qt 依赖）
precess_add_test(TestJobProgressSlot TestJobProgressSlot.cpp)
target_include_directories(TestJobProgressSlot PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../model")

# 单槽任务的 Qt 状态合流，直接编译展示对象以避免链接应用目标。
precess_add_test(TestQTaskStatus
    TestQTaskStatus.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../model/QTaskStatus.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../model/QTaskStatus.h"
)
set_target_properties(TestQTaskStatus PROPERTIES AUTOMOC ON)
target_include_directories(TestQTaskStatus PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../model")
precess_test_link_libraries(TestQTaskStatus Session Qt${VTK_QT_VERSION}::Core Qt${VTK_QT_VERSION}::Qml)

# QML 选择器参数的空值语义与功能系统同步
precess_add_test(TestSelectionParameter TestSelectionParameter.cpp)
precess_test_link_libraries(TestSelectionParameter modelQml QArgType FeatureSystemQml FeatureSystem Qt6::Qml)

precess_add_test(TestQModelDeletion TestQModelDeletion.cpp)
precess_test_link_libraries(TestQModelDeletion modelQml Qt6::Qml)

# 加载实际界面文件验证删除拒绝和 undo 后的选择器失效，不启动渲染窗口。
precess_add_test(TestUndoQml TestUndoQml.cpp ../TreeModel.cpp ../TreeModel.h)
set_target_properties(TestUndoQml PROPERTIES AUTOMOC ON)
target_include_directories(TestUndoQml PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
precess_test_link_libraries(TestUndoQml modelQml modelQmlplugin SystemQmlplugin renderQmlplugin renderQml Qt6::QuickControls2)
qt_add_resources(TestUndoQml undo_ui
    PREFIX "/undo-ui"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/.."
    FILES ../FeatureNavigation.js ../Theme.qml ../SideBar.qml ../OperationSession.qml ../OperationParameterModel.qml ../ObjectTree.qml ../CentralRenderArea.qml ../Selector.qml ../AppMenu.qml)

# 验证主窗口实际遮罩的覆盖范围和鼠标拦截，保留状态栏取消入口。
precess_add_test(TestBusyMask TestBusyMask.cpp)
precess_test_link_libraries(TestBusyMask Qt6::QuickControls2)
qt_add_resources(TestBusyMask busy_mask
    PREFIX "/busy-mask"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/.."
    FILES ../Main.qml)

# 偏好设置面板运行时 smoke：紧凑布局、主题开关与壁纸动作按钮
precess_add_test(TestPreferencesWindow TestPreferencesWindow.cpp)
set_target_properties(TestPreferencesWindow PROPERTIES AUTOMOC ON)
precess_test_link_libraries(TestPreferencesWindow
    modelQml modelQmlplugin
    coreQml coreQmlplugin
    SystemQmlplugin
    Qt${VTK_QT_VERSION}::QuickControls2
    Qt${VTK_QT_VERSION}::Qml
    Qt${VTK_QT_VERSION}::Quick
)
qt_add_resources(TestPreferencesWindow preferences_ui
    PREFIX "/preferences-ui"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/.."
    FILES
        ../PreferencesWindow.qml
        ../PreferenceCategoryButton.qml
        ../ThemeModeSwitch.qml
        ../ThemedDialog.qml
        ../Theme.qml)

# 验证主窗口窗口级调色板：启动后动态创建的 Fusion 控件继承 Theme 令牌
precess_add_test(TestThemeWindowPalette TestThemeWindowPalette.cpp)
precess_test_link_libraries(TestThemeWindowPalette Qt6::QuickControls2 Qt6::Qml)
qt_add_resources(TestThemeWindowPalette theme_window
    PREFIX "/theme-window"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/.."
    FILES ../Main.qml)

# 加载实际工具栏，覆盖通用父入口和旧算法列表入口。
precess_add_test(TestToolbarNavigation TestToolbarNavigation.cpp)
precess_test_link_libraries(TestToolbarNavigation modelQml modelQmlplugin SystemQmlplugin renderQmlplugin renderQml Qt6::QuickControls2 Qt6::Svg)
qt_add_resources(TestToolbarNavigation navigation_ui
    PREFIX "/navigation-ui"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/.."
    FILES ../SideBar.qml ../OperationSession.qml ../OperationParameterModel.qml ../AppToolbar.qml ../FeatureNavigation.js ../RibbonTabButton.qml ../RibbonActionButton.qml ../Theme.qml ../AppMenu.qml)
# 实际工具栏读取产品与插件声明的图标；资源随真实 QML 一同加载。
qt_add_resources(TestToolbarNavigation navigation_icons
    PREFIX "/images"
    BASE "${CMAKE_SOURCE_DIR}/resource"
    FILES
        "${CMAKE_SOURCE_DIR}/resource/toolbar/Algorithm/cmd.svg"
        "${CMAKE_SOURCE_DIR}/resource/toolbar/Algorithm/tetgen.svg"
        "${CMAKE_SOURCE_DIR}/resource/toolbar/Algorithm/gmsh.svg"
        "${CMAKE_SOURCE_DIR}/resource/toolbar/Mesh/triangle-meshing.svg"
        "${CMAKE_SOURCE_DIR}/resource/toolbar/Mesh/quad-meshing.svg"
        "${CMAKE_SOURCE_DIR}/resource/toolbar/Mesh/hexa-meshing.svg"
        "${CMAKE_SOURCE_DIR}/resource/toolbar/Geometry/split-edge.svg"
        "${CMAKE_SOURCE_DIR}/resource/toolbar/Geometry/merge-edge.svg"
        "${CMAKE_SOURCE_DIR}/resource/toolbar/Geometry/split-face.svg"
        "${CMAKE_SOURCE_DIR}/resource/toolbar/Geometry/merge-face.svg"
        "${CMAKE_SOURCE_DIR}/resource/toolbar/Geometry/merge-points.svg"
        "${CMAKE_SOURCE_DIR}/resource/toolbar/Geometry/collapse-edge.svg"
        "${CMAKE_SOURCE_DIR}/resource/toolbar/Geometry/patch-face.svg"
        "${CMAKE_SOURCE_DIR}/resource/toolbar/Geometry/fill-gap.svg"
        "${CMAKE_SOURCE_DIR}/resource/toolbar/Geometry/auto-geometry-repair.svg"
        "${CMAKE_SOURCE_DIR}/resource/toolbar/Geometry/geometry-reconstruction.svg"
        "${CMAKE_SOURCE_DIR}/resource/toolbar/Panels/preview.svg")

# Theme.qml 角色键契约与壁纸失败降级
precess_add_test(TestThemeQml TestThemeQml.cpp)
target_include_directories(TestThemeQml PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
precess_test_link_libraries(TestThemeQml
    Qt${VTK_QT_VERSION}::Core
    Qt${VTK_QT_VERSION}::Gui
    Qt${VTK_QT_VERSION}::Qml
    Qt${VTK_QT_VERSION}::Quick
)
qt_add_resources(TestThemeQml theme_qml
    PREFIX "/theme-qml"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/.."
    FILES ../Theme.qml)

# AppPalette 角色表适配与应用级调色板刷新
precess_add_test(TestAppPalette TestAppPalette.cpp ../core/AppPalette.h)
set_target_properties(TestAppPalette PROPERTIES AUTOMOC ON)
target_include_directories(TestAppPalette PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
precess_test_link_libraries(TestAppPalette Qt${VTK_QT_VERSION}::Gui Qt${VTK_QT_VERSION}::Qml)

# DockTheme 只保存宿主推送的配色表并通知
precess_add_test(TestDockTheme TestDockTheme.cpp ../dock/ui/DockTheme.h)
set_target_properties(TestDockTheme PROPERTIES AUTOMOC ON)
target_include_directories(TestDockTheme PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../dock")
precess_test_link_libraries(TestDockTheme Qt${VTK_QT_VERSION}::Core Qt${VTK_QT_VERSION}::Gui)

# 主题配色管道：同一份角色表到达应用调色板与停靠主题
precess_add_test(TestThemePipeline
    TestThemePipeline.cpp
    ../core/AppPalette.h
    ../dock/ui/DockTheme.h
)
set_target_properties(TestThemePipeline PROPERTIES AUTOMOC ON)
target_include_directories(TestThemePipeline PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/.." "${CMAKE_CURRENT_SOURCE_DIR}/../dock")
precess_test_link_libraries(TestThemePipeline Qt${VTK_QT_VERSION}::Core Qt${VTK_QT_VERSION}::Gui Qt${VTK_QT_VERSION}::Qml)

# 浮窗公共祖先的普通控件调色板继承
precess_add_test(TestDockWindowPalette TestDockWindowPalette.cpp)
target_include_directories(TestDockWindowPalette PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../dock")
precess_test_link_libraries(TestDockWindowPalette
    dock
    Qt${VTK_QT_VERSION}::QuickControls2
    Qt${VTK_QT_VERSION}::Qml
)

# WallpaperLibrary 壁纸库：导入校验、单张替换、移除与启动校验
precess_add_test(TestWallpaperLibrary
    TestWallpaperLibrary.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../core/WallpaperLibrary.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../core/WallpaperLibrary.h"
)
set_target_properties(TestWallpaperLibrary PROPERTIES AUTOMOC ON)
target_include_directories(TestWallpaperLibrary PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../core")
precess_test_link_libraries(TestWallpaperLibrary
    Qt${VTK_QT_VERSION}::Core
    Qt${VTK_QT_VERSION}::Gui
    Qt${VTK_QT_VERSION}::Quick
    spdlog::spdlog
)

# 带 EXIF Orientation=6 的测试图片（400x200，显示方向 200x400）
qt_add_resources(TestWallpaperLibrary wallpaper_test_data
    PREFIX "/test-data"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/data"
    FILES data/wallpaper_exif6.jpg)

# 壁纸背景组件：淡入交接、清除与失败回退
precess_add_test(TestWallpaperBackground TestWallpaperBackground.cpp)
precess_test_link_libraries(TestWallpaperBackground Qt6::Quick Qt6::Qml)
qt_add_resources(TestWallpaperBackground wallpaper_background
    PREFIX "/wallpaper-bg"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/.."
    FILES ../WallpaperBackground.qml)
