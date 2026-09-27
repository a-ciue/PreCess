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
