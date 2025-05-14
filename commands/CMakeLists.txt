# command/CMakeLists.txt
cmake_minimum_required(VERSION 3.14)

qt_add_library(commands STATIC
        SplitFaceCommand.cpp
        SplitEdgeCommand.cpp
        MergeBlocksCommand.cpp
        # LoadSplineCommand.cpp
        CommandDispatcher.cpp
)
qt_add_qml_module(commands
    URI commands
    VERSION 1.0
    SOURCES
        QCommandCatalog.h QCommand.h CommandDispatcher.h CommandDispatcher.cpp ArgTypeObject.h
)

target_link_libraries(commands PRIVATE Qt6::Gui)
#target_link_libraries(commands PRIVATE modelPart)  # 与 ModelData/ModelManager 库关联
#target_include_directories(commands PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
