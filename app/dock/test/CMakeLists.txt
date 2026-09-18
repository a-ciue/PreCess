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
