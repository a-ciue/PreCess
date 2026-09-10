add_custom_target(AllPlugins)
set(PLUGINS_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}")

# 从插件头文件解析插件类名（IID 形如 com.PreCess.systems.<分类>.<类名>/1.0）。
function(_get_plugin_class_name PLUGIN_H OUT_VAR)
    if(NOT IS_ABSOLUTE "${PLUGIN_H}")
        set(PLUGIN_H "${CMAKE_CURRENT_SOURCE_DIR}/${PLUGIN_H}")
    endif()
    file(READ "${PLUGIN_H}" plugin_h_content)
    if(plugin_h_content MATCHES "com\\.PreCess\\.systems\\.[A-Za-z]+\\.([A-Za-z0-9_]+)/")
        set(${OUT_VAR} "${CMAKE_MATCH_1}" PARENT_SCOPE)
    else()
        set(${OUT_VAR} "" PARENT_SCOPE)
    endif()
endfunction()

# 装配插件目标：默认产出动态插件 DLL；声明 STATIC 产出静态插件并记录到全局属性 STATIC_PLUGINS 中
# （wasm 平台不支持运行时动态加载插件，无论声明一律强制静态编译并打印警告）
# _add_plugin(<target> PLUGIN_H <headerPlugin.h> SOURCES <sources>... LIBS <libs>... [STATIC])
function(_add_plugin TARGET)
    cmake_parse_arguments(
        "plug"
        "STATIC"
        "PLUGIN_H"
        "SOURCES;LIBS"
        ${ARGN}
    )
    set(target_lib ${TARGET}lib)

    if(EMSCRIPTEN AND NOT plug_STATIC)
        message(WARNING "wasm 平台不支持运行时动态加载插件，插件 ${TARGET} 已强制编译为静态插件")
        set(plug_STATIC TRUE)
    endif()

    # 静态库，供测试链接使用
    add_library(${target_lib} STATIC ${plug_SOURCES})
    target_link_libraries(${target_lib} PUBLIC
        Core Data DataOps ${plug_LIBS}
        spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>
    )
    target_include_directories(${target_lib} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

    if(plug_STATIC)
        _get_plugin_class_name("${plug_PLUGIN_H}" plugin_class)
        if(NOT plugin_class)
            message(FATAL_ERROR "无法从 ${plug_PLUGIN_H} 解析插件类名（IID 应形如 com.PreCess.systems.<分类>.<类名>/1.0）")
        endif()
        qt_add_plugin(${TARGET} STATIC CLASS_NAME ${plugin_class} ${plug_PLUGIN_H})
        set_property(GLOBAL APPEND PROPERTY STATIC_PLUGINS "${TARGET}:${plugin_class}")
    else()
        qt_add_plugin(${TARGET} ${plug_PLUGIN_H})
        set_target_properties(${TARGET} PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY "${PLUGINS_BUILD_DIR}"
        )
        install(TARGETS ${TARGET}
            LIBRARY DESTINATION plugins
            COMPONENT AllPlugins
        )
    endif()
    target_link_libraries(${TARGET} PRIVATE
        ${target_lib}
        Qt6::Core
    )
    set_property(TARGET ${TARGET} PROPERTY PLUGIN_STATIC ${plug_STATIC})
    add_dependencies(AllPlugins ${TARGET})
endfunction()

# 各系统注册函数的公共装配逻辑在 _add_plugin 中，此处只传入各自的
# 系统差异链接库（算法插件经 IOSystem 头文件与模型 IO 系统交换数据）

# precess_add_io_plugin(<target>
#     SOURCES <source1> <source2> ...
#     PLUGIN_H <headerPlugin.h>
#     [STATIC]
# )
function(precess_add_io_plugin TARGET)
    _add_plugin(${TARGET} ${ARGN} LIBS $<COMPILE_ONLY:IOSystem>)
endfunction()

# precess_add_algo_plugin(<target>
#     SOURCES <source1> <source2> ...
#     PLUGIN_H <headerPlugin.h>
#     [STATIC]
# )
function(precess_add_algo_plugin TARGET)
    _add_plugin(${TARGET} ${ARGN} LIBS AlgorithmSystem $<COMPILE_ONLY:IOSystem>)
endfunction()

# precess_add_edit_plugin(<target>
#     SOURCES <source1> <source2> ...
#     PLUGIN_H <headerPlugin.h>
#     [STATIC]
# )
function(precess_add_edit_plugin TARGET)
    _add_plugin(${TARGET} ${ARGN} LIBS EditSystem)
endfunction()

# precess_add_feature_plugin(<target>
#     SOURCES <source1> <source2> ...
#     PLUGIN_H <headerPlugin.h>
#     [STATIC]
# )
function(precess_add_feature_plugin TARGET)
    _add_plugin(${TARGET} ${ARGN} LIBS FeatureSystem)
endfunction()

# precess_plugin_link_libraries(<target> <lib1> <lib2> ...)
function(precess_plugin_link_libraries TARGET)
    if(ARGC EQUAL 0)
        message(FATAL_ERROR "Function my_function requires at least 1 arguments, but got ${ARGC}")
    endif()
    set(DEPENDENT_LIBRARIES ${ARGN})

    target_link_libraries(${TARGET}lib PUBLIC ${DEPENDENT_LIBRARIES})

    get_property(plug_is_static TARGET ${TARGET} PROPERTY PLUGIN_STATIC)
    if(plug_is_static)
        return()
    endif()

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

# 生成静态插件导入文件，作为接口库 StaticPlugins 被主程序链接
get_property(static_plugins GLOBAL PROPERTY STATIC_PLUGINS)
if(NOT static_plugins)
    return()
endif()
set(imports_content
"// Generated by CMake, DO NOT EDIT\n#include <QtCore/qplugin.h>\n")
set(static_plugin_targets "")
foreach(plugin_entry IN LISTS static_plugins)
    string(REPLACE ":" ";" plugin_entry_parts "${plugin_entry}")
    list(GET plugin_entry_parts 0 plugin_target)
    list(GET plugin_entry_parts 1 plugin_class)
    string(APPEND imports_content "Q_IMPORT_PLUGIN(${plugin_class})\n")
    list(APPEND static_plugin_targets "${plugin_target}")
endforeach()
set(imports_file "${CMAKE_CURRENT_BINARY_DIR}/StaticPluginsLoader.cpp")
# 内容相同时不重写，避免每次 configure 都触发重编
if(EXISTS "${imports_file}")
    file(READ "${imports_file}" existing_imports_content)
endif()
if(NOT existing_imports_content STREQUAL imports_content)
    file(WRITE "${imports_file}" "${imports_content}")
endif()
add_library(StaticPlugins INTERFACE)
target_sources(StaticPlugins INTERFACE "${imports_file}")
target_link_libraries(StaticPlugins INTERFACE ${static_plugin_targets})
list(LENGTH static_plugin_targets plugin_count)
message(STATUS "found static plugins (${plugin_count}): ${static_plugin_targets}")
