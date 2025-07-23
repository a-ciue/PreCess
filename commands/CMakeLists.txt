# command/CMakeLists.txt
cmake_minimum_required(VERSION 3.14)

# 准备进行枚举类型 ArgTypeEnum 源代码注入
file(READ "ArgTypeEnum.h" ArgTypeEnum_H_CONTENT)
set_directory_properties(PROPERTIES CMAKE_CONFIGURE_DEPENDS "ArgTypeEnum.h")    # ArgTypeEnum.h更新时能触发重新配置
configure_file("QArgType.h.in" "QArgType.h")
add_library(QArgType INTERFACE)
target_include_directories(QArgType INTERFACE "${CMAKE_CURRENT_BINARY_DIR}")

qt_add_library(commands STATIC
        # SplitFaceCommand.cpp
        # SplitEdgeCommand.cpp
        # RemeshBlockCommand.cpp
        MergeBlocksCommand.cpp
        # LoadSplineCommand.cpp
        # ImportModelCommand.cpp
        CommandDispatcher.cpp
        TrianglulationCommand.cpp
        CmdExecuteCommand.cpp
)
target_include_directories(commands PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")
qt_add_qml_module(commands
    URI commands
    VERSION 1.0
    SOURCES
        QCommandCatalog.h QCommand.h CommandDispatcher.h CommandDispatcher.cpp "${CMAKE_CURRENT_BINARY_DIR}/QArgType.h" ImportModelCommand.h 
)
target_include_directories(commandsplugin PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(commands PRIVATE Qt6::Gui ${OpenCASCADE_LIBRARIES})
#target_link_libraries(commands PRIVATE modelPart)  # 与 ModelData/ModelManager 库关联
#target_include_directories(commands PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
