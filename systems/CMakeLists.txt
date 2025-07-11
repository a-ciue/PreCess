add_subdirectory(io)

add_library(Systems INTERFACE)
target_link_libraries(Systems INTERFACE IOSystem)