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
    FILES ../Theme.qml ../SideBar.qml ../ObjectTree.qml ../CentralRenderArea.qml ../Selector.qml)

# 验证主窗口实际遮罩的覆盖范围和鼠标拦截，保留状态栏取消入口。
precess_add_test(TestBusyMask TestBusyMask.cpp)
precess_test_link_libraries(TestBusyMask Qt6::QuickControls2)
qt_add_resources(TestBusyMask busy_mask
    PREFIX "/busy-mask"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/.."
    FILES ../Main.qml)
