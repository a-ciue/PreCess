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
# app 层无库目标（QPythonRuntime 编入 modelQml QML 模块），同 TestQLogManager
# 直接编译源文件并启用 AUTOMOC；QPythonAppModule 按 precess_bindings 目标存在
# 性二选一（真实现含 pybind11，桩为空操作），与 modelQml 同源；Python 嵌入
# 不可用（桩实现）时用例自行跳过
set(_qpython_runtime_test_sources
    TestQPythonRuntime.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../model/QPythonRuntime.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../model/QPythonRuntime.h"
)
if(TARGET precess_bindings)
    find_package(pybind11 CONFIG QUIET) # python/ 已发现过，此处取回导入目标
    list(APPEND _qpython_runtime_test_sources
        "${CMAKE_CURRENT_SOURCE_DIR}/../model/QPythonAppModule.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../model/QPythonAppModule.h")
else()
    list(APPEND _qpython_runtime_test_sources
        "${CMAKE_CURRENT_SOURCE_DIR}/../model/QPythonAppModule_stub.cpp")
endif()
precess_add_test(TestQPythonRuntime ${_qpython_runtime_test_sources})
set_target_properties(TestQPythonRuntime PROPERTIES AUTOMOC ON)
target_include_directories(TestQPythonRuntime PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../model")
precess_test_link_libraries(TestQPythonRuntime
    Qt${VTK_QT_VERSION}::Core
    Qt${VTK_QT_VERSION}::Qml
    spdlog::spdlog
    precess_runtime
)
if(TARGET precess_bindings AND BUILD_TESTING)
    target_link_libraries(TestQPythonRuntime PRIVATE pybind11::embed)
endif()
# 运行时宿主配置宏：与 modelQml 同源（真实模式才定义，桩模式以空值兜底）
if(TARGET precess_bindings)
    target_compile_definitions(TestQPythonRuntime PRIVATE
        PRECESS_PYTHON_HOME="${PRECESS_PYTHON_HOME_DIR}"
        PRECESS_PYTHON_MODULE_DIR="$<TARGET_FILE_DIR:precess_bindings>"
    )
endif()
