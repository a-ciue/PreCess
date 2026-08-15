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
        "${CMAKE_CURRENT_BINARY_DIR}/QArgType.h"
        QArgObject.h
        QArgObject.cpp
        QGeometryOperations.h
        QGeometryOperations.cpp
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

add_subdirectory(systems)
