# command/CMakeLists.txt
cmake_minimum_required(VERSION 3.14)

# 准备进行枚举类型 ArgTypeEnum 源代码注入
file(READ "ArgTypeEnum.h" ArgTypeEnum_H_CONTENT)
set_directory_properties(PROPERTIES CMAKE_CONFIGURE_DEPENDS "ArgTypeEnum.h")    # ArgTypeEnum.h更新时能触发重新配置
configure_file("QArgType.h.in" "QArgType.h")
add_library(QArgObject INTERFACE)
target_include_directories(QArgObject INTERFACE "${CMAKE_CURRENT_BINARY_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")

qt_add_library(commands STATIC)
qt_add_qml_module(commands
    URI commands
    VERSION 1.0
    SOURCES
        "${CMAKE_CURRENT_BINARY_DIR}/QArgType.h" QArgObject.h QArgObject.cpp
)
target_link_libraries(commands PUBLIC QArgObject)
target_link_libraries(commands PRIVATE Data Qt6::Gui ${OpenCASCADE_LIBRARIES})
#target_include_directories(commands PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
