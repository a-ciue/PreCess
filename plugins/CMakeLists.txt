add_custom_target(AllPlugins)
set(PLUGINS_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}")

# precess_add_io_plugin(<target> 
#     SOURCES <source1> <source2> ...
#     PLUGIN_H <headerPlugin.h>
# )
function(precess_add_io_plugin TARGET)
    cmake_parse_arguments(
        "plug"
        ""
        "PLUGIN_H"
        "SOURCES"
        ${ARGN}
    )
    set(target_lib ${TARGET}lib)

    # 静态库，供测试链接使用
    add_library(${target_lib} STATIC ${plug_SOURCES})
    target_link_libraries(${target_lib} PUBLIC
        Core Data DataOps
        $<COMPILE_ONLY:IOSystem>
        spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>
    )
    target_include_directories(${target_lib} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

    # 插件
    qt_add_plugin(${TARGET} ${plug_PLUGIN_H})
    target_link_libraries(${TARGET} PRIVATE
        ${target_lib}
        Qt6::Core
    )
    set_target_properties(${TARGET} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${PLUGINS_BUILD_DIR}"
    )
    add_dependencies(AllPlugins ${TARGET})

    install(TARGETS ${TARGET}
        LIBRARY DESTINATION plugins
        COMPONENT AllPlugins
    )
endfunction()

# precess_add_algo_plugin(<target> 
#     SOURCES <source1> <source2> ...
#     PLUGIN_H <headerPlugin.h>
# )
function(precess_add_algo_plugin TARGET)
    cmake_parse_arguments(
        "plug"
        ""
        "PLUGIN_H"
        "SOURCES"
        ${ARGN}
    )
    set(target_lib ${TARGET}lib)

    # 静态库，供测试链接使用
    add_library(${target_lib} STATIC ${plug_SOURCES})
    target_link_libraries(${target_lib} PUBLIC
        Core Data DataOps AlgorithmSystem
        $<COMPILE_ONLY:IOSystem>
        spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>
    )
    target_include_directories(${target_lib} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

    # 插件
    qt_add_plugin(${TARGET} ${plug_PLUGIN_H})
    target_link_libraries(${TARGET} PRIVATE
        ${target_lib}
        Qt6::Core
    )
    set_target_properties(${TARGET} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${PLUGINS_BUILD_DIR}"
    )
    add_dependencies(AllPlugins ${TARGET})

    install(TARGETS ${TARGET}
        LIBRARY DESTINATION plugins
        COMPONENT AllPlugins
    )
endfunction()

# precess_add_edit_plugin(<target> 
#     SOURCES <source1> <source2> ...
#     PLUGIN_H <headerPlugin.h>
# )
function(precess_add_edit_plugin TARGET)
    cmake_parse_arguments(
        "plug"
        ""
        "PLUGIN_H"
        "SOURCES"
        ${ARGN}
    )
    set(target_lib ${TARGET}lib)

    # 静态库，供测试链接使用
    add_library(${target_lib} STATIC ${plug_SOURCES})
    target_link_libraries(${target_lib} PUBLIC
        Core Data DataOps EditSystem
        spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>
    )
    target_include_directories(${target_lib} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

    # 插件
    qt_add_plugin(${TARGET} ${plug_PLUGIN_H})
    target_link_libraries(${TARGET} PRIVATE
        ${target_lib}
        Qt6::Core
    )
    set_target_properties(${TARGET} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${PLUGINS_BUILD_DIR}"
    )
    add_dependencies(AllPlugins ${TARGET})

    install(TARGETS ${TARGET}
        LIBRARY DESTINATION plugins
        COMPONENT AllPlugins
    )
endfunction()

# precess_add_feature_plugin(<target> 
#     SOURCES <source1> <source2> ...
#     PLUGIN_H <headerPlugin.h>
# )
function(precess_add_feature_plugin TARGET)
    cmake_parse_arguments(
        "plug"
        ""
        "PLUGIN_H"
        "SOURCES"
        ${ARGN}
    )
    set(target_lib ${TARGET}lib)

    # 静态库，供测试链接使用
    add_library(${target_lib} STATIC ${plug_SOURCES})
    target_link_libraries(${target_lib} PUBLIC
        Core Data DataOps FeatureSystem
        spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>
    )
    target_include_directories(${target_lib} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

    # 插件
    qt_add_plugin(${TARGET} ${plug_PLUGIN_H})
    target_link_libraries(${TARGET} PRIVATE
        ${target_lib}
        Qt6::Core
    )
    set_target_properties(${TARGET} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${PLUGINS_BUILD_DIR}"
    )
    add_dependencies(AllPlugins ${TARGET})

    install(TARGETS ${TARGET}
        LIBRARY DESTINATION plugins
        COMPONENT AllPlugins
    )
endfunction()

# precess_plugin_link_libraries(<target> <lib1> <lib2> ...)
function(precess_plugin_link_libraries TARGET)
    if(ARGC EQUAL 0)
        message(FATAL_ERROR "Function my_function requires at least 1 arguments, but got ${ARGC}")
    endif()
    set(DEPENDENT_LIBRARIES ${ARGN})

    target_link_libraries(${TARGET}lib PUBLIC ${DEPENDENT_LIBRARIES})

    if (OpenCASCADE_FOUND)
        list(APPEND DEPENDENT_LIBRARIES TKernel)
        if (freetype_FOUND)
            list(APPEND DEPENDENT_LIBRARIES freetype)
        endif()
    endif()
    list(TRANSFORM DEPENDENT_LIBRARIES PREPEND "$<TARGET_FILE_DIR:")
    list(TRANSFORM DEPENDENT_LIBRARIES APPEND ">")	
    install(
            TARGETS ${TARGET}
            RUNTIME_DEPENDENCIES
            DIRECTORIES $<TARGET_FILE_DIR:Qt${VTK_QT_VERSION}::Core> ${DEPENDENT_LIBRARIES}
            PRE_EXCLUDE_REGEXES "api-ms-" "ext-ms-" "spdlog" "freetype" # don't install Windows-provided libs
            POST_EXCLUDE_REGEXES ".*system32/.*\\.dll" "Qt${VTK_QT_VERSION}Core"  # don't install Windows-provided libs
            FRAMEWORK DESTINATION Frameworks
            BUNDLE DESTINATION .
            LIBRARY DESTINATION plugins
            COMPONENT AllPlugins
    )
endfunction()

add_subdirectory(shared/cgal_support)
add_subdirectory(io)
add_subdirectory(algo)
add_subdirectory(edit)
add_subdirectory(feature)