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

# 内嵌 Python 运行环境（可选）：Python3 + pybind11 就绪（且非 wasm）时编译
# QPythonRuntime 真实实现，否则编译降级实现，运行时 available 恒 false
set(PRECESS_EMBED_PYTHON OFF)
if(PRECESS_BUILD_PYTHON AND NOT EMSCRIPTEN)
    find_package(Python3 COMPONENTS Interpreter Development.Module Development.Embed QUIET)
    if(Python3_FOUND)
        find_package(pybind11 CONFIG QUIET)
        if(pybind11_FOUND)
            set(PRECESS_EMBED_PYTHON ON)
        else()
            message(STATUS "未找到 pybind11，app 以无内嵌 Python 运行环境模式构建")
        endif()
    else()
        message(STATUS "未找到 Python3 嵌入式开发库，app 以无内嵌 Python 运行环境模式构建")
    endif()
endif()

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
target_link_libraries(modelQml PRIVATE 
    modelPart QArgType
)

if(PRECESS_EMBED_PYTHON)
    target_link_libraries(modelQml PRIVATE pybind11::embed)
    get_filename_component(PRECESS_PYTHON_HOME_DIR "${Python3_EXECUTABLE}" DIRECTORY)
    target_compile_definitions(modelQml PRIVATE
        PRECESS_EMBED_PYTHON
        # 嵌入式解释器的标准库根：构建机解释器目录（安装分发场景见后续打包阶段）
        PRECESS_PYTHON_HOME="${PRECESS_PYTHON_HOME_DIR}"
        # 构建树中 precess 扩展模块输出目录（运行时另一候选为 <exe_dir>/python）
        PRECESS_PYTHON_MODULE_DIR="$<TARGET_FILE_DIR:precess>"
    )
endif()

add_subdirectory(systems)
