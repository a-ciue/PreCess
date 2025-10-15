add_library(DataTest STATIC MakeMeshData.cpp)
target_link_libraries(DataTest PUBLIC Data)
target_include_directories(DataTest PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

