# command/CMakeLists.txt
cmake_minimum_required(VERSION 3.14)

add_library(commands STATIC
        ICommand.h
        SplitFaceCommand.h SplitFaceCommand.cpp
        MergeBlocksCommand.h MergeBlocksCommand.cpp
        LoadSplineCommand.h LoadSplineCommand.cpp
        CommandDispatcher.h CommandDispatcher.cpp
        QCommand.h
)

target_link_libraries(commands PRIVATE Qt6::Core Qt6::Quick)
target_link_libraries(commands PRIVATE modelPart)  # 与 ModelData/ModelManager 库关联
target_include_directories(commands PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
