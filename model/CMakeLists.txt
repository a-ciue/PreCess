add_subdirectory(data)
add_subdirectory(systems)
add_subdirectory(ops)

add_library(modelPart STATIC
    FileHandler.cpp
)
target_include_directories(modelPart
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}   # 项目根目录
)
target_link_libraries(modelPart PUBLIC
    Systems
    Data
    DataOps
)
target_link_libraries(modelPart PRIVATE
    TKDESTEP      # OCCT 的库列表
)