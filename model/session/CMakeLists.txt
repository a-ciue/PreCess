add_library(Session STATIC
    Session.cpp
    SessionQuery.cpp
)
target_include_directories(Session PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(Session PUBLIC
    Core
    Data
    DataOps
    Systems
)
if (OpenCASCADE_FOUND)
    target_link_libraries(Session PUBLIC $<COMPILE_ONLY:TKernel>)
    target_link_libraries(Session PRIVATE TKernel TKTopAlgo)
endif()
# SystemPluginManager.h（model/systems 插件管线）尚依赖 Qt 头：仅 Session.cpp 编译期需要，
# Session.h 保持无 Qt；插件管线解耦后移除
target_link_libraries(Session PRIVATE Qt6::Core)

add_subdirectory(test)
