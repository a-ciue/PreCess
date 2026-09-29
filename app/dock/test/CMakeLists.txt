# 布局引擎单元测试
precess_add_test(TestDockLayout
    TestDockLayout.cpp
)
precess_test_link_libraries(TestDockLayout
    dock
    Qt${VTK_QT_VERSION}::Core
)

# 停靠核心语义单元测试
precess_add_test(TestDockCore
    TestDockCore.cpp
)
precess_test_link_libraries(TestDockCore
    dock
    Qt${VTK_QT_VERSION}::Core
)

# 拖放核心单元测试
precess_add_test(TestDockDragDrop
    TestDockDragDrop.cpp
)
precess_test_link_libraries(TestDockDragDrop
    dock
    Qt${VTK_QT_VERSION}::Core
)

# 布局持久化测试
precess_add_test(TestDockPersist
    TestDockPersist.cpp
)
precess_test_link_libraries(TestDockPersist
    dock
    Qt${VTK_QT_VERSION}::Core
)

# QML 集成冒烟测试（offscreen 平台）
find_package(Qt${VTK_QT_VERSION} REQUIRED COMPONENTS QuickTest)
precess_add_test(TestDockQmlSmoke
    TestDockQmlSmoke.cpp
)
precess_test_link_libraries(TestDockQmlSmoke
    dock
    Qt${VTK_QT_VERSION}::Core
    Qt${VTK_QT_VERSION}::Gui
    Qt${VTK_QT_VERSION}::Quick
    Qt${VTK_QT_VERSION}::Test
    Qt${VTK_QT_VERSION}::QuickTest
)
qt_add_resources(TestDockQmlSmoke "dock_test_qml"
    PREFIX "/precess/dock/test"
    FILES CentralStub.qml
)
