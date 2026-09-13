# QLogManager Qt/QML 消息桥接测试
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
