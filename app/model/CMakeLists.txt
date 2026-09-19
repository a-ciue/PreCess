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
        QPythonRuntimeQml.h
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
    modelPart QArgType precess_runtime pythonApp
)

# app 侧 Python 运行时接入（静态库，恒存在）
qt_add_library(pythonApp STATIC
    QPythonRuntime.cpp
    QPythonRuntime.h
)
if(TARGET precess_bindings)
    find_package(pybind11 CONFIG QUIET) # python/ 已发现过，此处取回导入目标
    target_sources(pythonApp PRIVATE QPythonAppModule.cpp)
    target_link_libraries(pythonApp PRIVATE pybind11::embed)
else()
    target_sources(pythonApp PRIVATE QPythonAppModule_stub.cpp)
endif()
target_include_directories(pythonApp PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
# PUBLIC：暴露 session::Session*，消费方需 Session 头/链接与宿主配置宏
target_link_libraries(pythonApp PUBLIC precess_runtime)
target_link_libraries(pythonApp PRIVATE
    Qt${VTK_QT_VERSION}::Core
    spdlog::spdlog
)

add_subdirectory(systems)
