add_subdirectory(data)

add_subdirectory(qt)

add_library(modelPart STATIC
        ModelUtil.cpp
        FileHandler.cpp
        

)

# 让 modelPart 自己也能看到 VTK 和 Qt 的头文件
target_include_directories(modelPart
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}   # 项目根目录
)
target_link_libraries(modelPart
        PRIVATE
        ${OpenCASCADE_LIBRARIES}      # OCCT 的库列表
        Systems
        Data
)

add_subdirectory(systems)
add_subdirectory(plugins)
target_link_libraries(modelQml PRIVATE modelPart commands)