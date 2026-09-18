# 准备进行枚举类型 ArgTypeEnum 源代码注入
get_target_property(ArgTypeEnum_DIRS Core INCLUDE_DIRECTORIES)
find_file(ArgTypeEnum_DIR "private_ArgTypeEnum.h" 
    PATHS ${ArgTypeEnum_DIRS}
    NO_CACHE # 非缓存变量，每次配置时重新求值保证变量最新
)
file(READ ${ArgTypeEnum_DIR} ArgTypeEnum_H_CONTENT)
set_directory_properties(PROPERTIES CMAKE_CONFIGURE_DEPENDS ${ArgTypeEnum_DIR})    # private_ArgTypeEnum.h更新时能触发重新配置
configure_file("QArgType.h.in" "QArgType.h")
add_library(QArgType INTERFACE)
target_include_directories(QArgType INTERFACE "${CMAKE_CURRENT_BINARY_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")

# modelQml目标
qt_add_qml_module(modelQml 
    URI app.model
    STATIC
    SOURCES
        QModelObserver.h
        QModelQuery.h
        QModelQuery.cpp
        QModelManager.h
        QModelManager.cpp
        QPythonRuntime.h
        QPythonRuntime.cpp
        "${CMAKE_CURRENT_BINARY_DIR}/QArgType.h"
        QArgObject.h
        QArgObject.cpp
        QUndoStackAdaptor.h
        QUndoStackAdaptor.cpp
)
target_include_directories(modelQml PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}   # 头文件路径
)
# 将插件暴露给使用者（PUBLIC）
target_link_libraries(modelQmlplugin PUBLIC 
    coreQmlplugin SystemQml
)
# 将封装接口暴露给使用者（PUBLIC），隐藏C++实现(PRIVATE)
target_link_libraries(modelQml PUBLIC 
    coreQml SystemQml
)
# 内嵌 Python 运行环境：precess_runtime 目标恒存在（python/ 提供，Python 依赖
# 缺失时自动降级为桩实现，"不可用"是运行时状态），宿主无需条件编译
target_link_libraries(modelQml PRIVATE
    modelPart QArgType precess_runtime pythonAppModule
)

# app 侧 Python 模块注册（precess_app）：注册逻辑在 app 经 pybind11 任意签名
# def（见 QPythonAppModule.cpp）。静态库目标恒存在，真实现/桩按可用性二选一
# （同 python/ 的 precess_runtime 模式，判断只在目标定义处一次），消费方直接
# 链接、无需重复判断；桩模式注册为空操作（"不可用"是运行时状态）
add_library(pythonAppModule STATIC)
if(TARGET precess_bindings)
    find_package(pybind11 CONFIG QUIET) # python/ 已发现过，此处取回导入目标
    target_sources(pythonAppModule PRIVATE QPythonAppModule.cpp)
    target_link_libraries(pythonAppModule PRIVATE pybind11::embed)
else()
    target_sources(pythonAppModule PRIVATE QPythonAppModule_stub.cpp)
endif()
target_include_directories(pythonAppModule PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(pythonAppModule PRIVATE
    Qt${VTK_QT_VERSION}::Core
    spdlog::spdlog
)

add_subdirectory(systems)
