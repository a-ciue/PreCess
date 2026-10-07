# PreCess 插件构建 API（in-tree / 外部 SDK 同源）
#
# 本文件随 PreCess 安装到 <prefix>/lib/cmake/PreCess/，由 PreCessConfig.cmake 自动
# include；in-tree 由 plugins/CMakeLists.txt include。两种语境下调用方写法完全一致：
#   precess_add_io_plugin / precess_add_algo_plugin /
#   precess_add_edit_plugin / precess_add_feature_plugin
#   precess_plugin_link_libraries   插件私有三方依赖（可选）
#   precess_plugin_install          安装到 PreCess 插件目录（外部工程必调；in-tree 自动安装）
#
# 语境差异（本文件内部消化，调用方无感）：
#   - in-tree：插件目标自动安装到 plugins/（macOS 为 .app/Contents/plugins）并挂接
#     AllPlugins 自定义目标；静态插件登记 STATIC_PLUGINS 供 StaticPlugins 装配。
#   - 外部：默认输出到 <build>/plugins，经 precess_plugin_install() 装入
#     PRECESS_PLUGIN_INSTALL_DIR（find_package(PreCess) 时解析）；仅支持动态插件。

include_guard(GLOBAL)

# 安装期 file(GET_RUNTIME_DEPENDENCIES)（即 RUNTIME_DEPENDENCIES）的 CMP0207：
# 未设策略时，路径规范化前做匹配会为每个扫到的系统 DLL 各刷一条 dev warning。
# 统一置 NEW（规范化后匹配，CMake ≥3.26 才有该策略，旧版跳过）
if(POLICY CMP0207)
    cmake_policy(SET CMP0207 NEW)
endif()

# ---- 语境与全局约定 -----------------------------------------------------------

# 主工程入口或 SDK 包入口负责初始化，不能由是否存在同名目标推断。
if(NOT PRECESS_PLUGIN_IN_TREE)
    if(NOT TARGET PreCess::Base)
        message(FATAL_ERROR
            "PreCessPlugin.cmake 需要 PreCess 目标：外部构建请先 find_package(PreCess REQUIRED)")
    endif()
    if(NOT DEFINED PRECESS_PLUGIN_INSTALL_DIR)
        message(FATAL_ERROR
            "PRECESS_PLUGIN_INSTALL_DIR 未定义：外部构建请经 find_package(PreCess) 加载本模块")
    endif()
endif()

if(NOT DEFINED PRECESS_APP_NAME)
    set(PRECESS_APP_NAME "PreCess")
endif()
if(NOT DEFINED PRECESS_QT_MAJOR)
    if(DEFINED VTK_QT_VERSION)
        set(PRECESS_QT_MAJOR "${VTK_QT_VERSION}")
    else()
        set(PRECESS_QT_MAJOR 6)
    endif()
endif()

# qt_add_plugin 来自 Qt6CoreMacros：外部工程 find_package(Qt6 COMPONENTS Core) 后
# 未必加载宏定义，兜底 include
if(NOT COMMAND qt_add_plugin)
    if(DEFINED Qt6_DIR AND EXISTS "${Qt6_DIR}/Qt6CoreMacros.cmake")
        include("${Qt6_DIR}/Qt6CoreMacros.cmake")
    elseif(DEFINED QT_INSTALL_DATA AND EXISTS "${QT_INSTALL_DATA}/cmake/Qt6CoreMacros.cmake")
        include("${QT_INSTALL_DATA}/cmake/Qt6CoreMacros.cmake")
    else()
        message(FATAL_ERROR "qt_add_plugin 不可用：请先 find_package(Qt6 COMPONENTS Core)")
    endif()
endif()

# 插件目录独立于实现源码所在的子目录；主工程可提供 macOS bundle 布局。
if(NOT DEFINED plugins_output_dir)
    set(plugins_output_dir "${CMAKE_BINARY_DIR}/plugins")
endif()

# ---- 内部辅助 -----------------------------------------------------------------

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

# 插件运行时依赖收拢的排除规则（in-tree 安装与 precess_plugin_install 共用）。
# 匹配语义（CMake file(GET_RUNTIME_DEPENDENCIES) 文档）：PRE 过滤依赖**名字**、
# POST 过滤解析后的**全路径**，且 Windows 下用于匹配的名称/路径**先转为小写**
# ——正则必须写成全小写 + `.*` 锚定（大写形式不会被匹配到，形同虚设）
# _precess_plugin_dep_exclude_regexes(<out_post> <out_pre>)
function(_precess_plugin_dep_exclude_regexes OUT_POST OUT_PRE)
    set(_pre ".*api-ms-.*" ".*ext-ms-.*" ".*spdlog.*" ".*freetype.*") # don't install Windows-provided libs
    set(_post ".*system32/.*\\.dll" ".*qt${PRECESS_QT_MAJOR}core.*")
    if(APPLE)
        list(APPEND _post
            "/usr/lib/.*"
            "/System/Library/.*"
        )
    elseif(UNIX)
        list(APPEND _post
            ".*libc\\.so.*"
            ".*libm\\.so.*"
            ".*libdl\\.so.*"
            ".*libpthread\\.so.*"
            ".*librt\\.so.*"
            ".*libnsl\\.so.*"
            ".*libstdc\\+\\+\\.so.*"
            ".*libgcc_s\\.so.*"
            ".*libGL\\.so.*"
            ".*libEGL\\.so.*"
            ".*libOpenGL\\.so.*"
            ".*libGLX\\.so.*"
            ".*libxkbcommon\\.so.*"
            ".*libwayland-.*\\.so.*"
            ".*libX[a-zA-Z0-9]*\\.so.*"
            ".*libfontconfig\\.so.*"
            ".*libglib-2\\.0\\.so.*"
        )
    endif()
    set(${OUT_POST} "${_post}" PARENT_SCOPE)
    set(${OUT_PRE} "${_pre}" PARENT_SCOPE)
endfunction()

# ---- 插件装配 -----------------------------------------------------------------

# 装配插件目标：默认产出动态插件 DLL；声明 STATIC 产出静态插件并记录到全局属性
# STATIC_PLUGINS 中（wasm 平台不支持运行时动态加载插件，无论声明一律强制静态编译）。
# _add_plugin(<target> PLUGIN_H <headerPlugin.h> SOURCES <sources>...)
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
    if(plug_STATIC AND NOT PRECESS_PLUGIN_IN_TREE)
        message(FATAL_ERROR
            "静态插件需链接进 PreCess 主程序（Q_IMPORT_PLUGIN），不支持独立构建：${TARGET}")
    endif()

    if(COMMAND _precess_check_abi)
        _precess_check_abi()
    endif()

    # 静态库，供测试链接使用；模型层整体经 PreCess::Base 提供（含各系统接口）
    add_library(${target_lib} STATIC ${plug_SOURCES})
    set_target_properties(${target_lib} PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_link_libraries(${target_lib} PUBLIC
        PreCess::Base ${plug_LIBS}
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
            RUNTIME_OUTPUT_DIRECTORY "${plugins_output_dir}"
            LIBRARY_OUTPUT_DIRECTORY "${plugins_output_dir}"
        )
        if(APPLE)
            set_property(TARGET ${TARGET} APPEND PROPERTY INSTALL_RPATH "@loader_path/../Frameworks")
        endif()
        # in-tree 自动安装；外部工程经 precess_plugin_install() 显式安装
        if(PRECESS_PLUGIN_IN_TREE)
            install(TARGETS ${TARGET}
                COMPONENT AllPlugins
                RUNTIME DESTINATION "${plugins_destination}"
                LIBRARY DESTINATION "${plugins_destination}"
            )
        endif()
    endif()
    target_link_libraries(${TARGET} PRIVATE
        ${target_lib}
        Qt6::Core
    )
    # Q_OBJECT/Q_PLUGIN_METADATA 需 moc：in-tree 由 qt_standard_project_setup 全局开启，
    # 外部工程无此设置，此处目标级兜底（两语境行为一致）
    set_target_properties(${TARGET} PROPERTIES AUTOMOC ON)
    set_property(TARGET ${TARGET} PROPERTY PLUGIN_STATIC ${plug_STATIC})
    if(TARGET AllPlugins)
        add_dependencies(AllPlugins ${TARGET})
    endif()
endfunction()

# 各系统注册函数的公共装配逻辑在 _add_plugin 中；模型层归并（PreCess::Base）后
# 系统差异链接库不再需要，保留函数名以兼容既有插件工程写法。

# precess_add_io_plugin(<target>
#     SOURCES <source1> <source2> ...
#     PLUGIN_H <headerPlugin.h>
#     [STATIC]
# )
function(precess_add_io_plugin TARGET)
    _add_plugin(${TARGET} ${ARGN})
endfunction()

# precess_add_algo_plugin(<target>
#     SOURCES <source1> <source2> ...
#     PLUGIN_H <headerPlugin.h>
#     [STATIC]
# )
function(precess_add_algo_plugin TARGET)
    _add_plugin(${TARGET} ${ARGN})
endfunction()

# precess_add_edit_plugin(<target>
#     SOURCES <source1> <source2> ...
#     PLUGIN_H <headerPlugin.h>
#     [STATIC]
# )
function(precess_add_edit_plugin TARGET)
    _add_plugin(${TARGET} ${ARGN})
endfunction()

# precess_add_feature_plugin(<target>
#     SOURCES <source1> <source2> ...
#     PLUGIN_H <headerPlugin.h>
#     [STATIC]
# )
function(precess_add_feature_plugin TARGET)
    _add_plugin(${TARGET} ${ARGN})
endfunction()

# precess_plugin_link_libraries(<target> <lib1> <lib2> ...)
# 插件私有三方依赖：链接进插件并（in-tree）安装时收拢其运行时 DLL；
# 外部工程由 precess_plugin_install() 统一收拢。
function(precess_plugin_link_libraries TARGET)
    if(ARGC LESS 2)
        message(FATAL_ERROR "precess_plugin_link_libraries 至少需要 1 个依赖库参数，但收到 ${ARGC} 个参数")
    endif()
    set(DEPENDENT_LIBRARIES ${ARGN})

    target_link_libraries(${TARGET}lib PUBLIC ${DEPENDENT_LIBRARIES})

    get_property(plug_is_static TARGET ${TARGET} PROPERTY PLUGIN_STATIC)
    if(plug_is_static)
        return()
    endif()

    if(NOT PRECESS_PLUGIN_IN_TREE)
        # 外部：仅记录依赖目标，收拢动作在 precess_plugin_install 中执行
        set_property(TARGET ${TARGET} APPEND PROPERTY PRECESS_PLUGIN_DEP_LIBS ${DEPENDENT_LIBRARIES})
        return()
    endif()

    # OCCT 扫描目录兜底：仅从显式传入依赖推导扫描目录并不够——
    # 未显式传 TK* 但经 PreCess::Base 链入 OCCT 符号的插件（如 VtkLegacyModelPlugin）
    # 在无 OCCT 于 PATH 的环境下安装即报 "Could not resolve runtime dependencies:
    # TKBRep.dll/TKernel.dll"。TKernel 的 IMPORTED_LOCATION 为 per-config DLL 路径
    # （Debug=bind、Release=bin），恒可作扫描目录；freetype/spdlog 本就 PRE_EXCLUDE 不收拢。
    list(TRANSFORM DEPENDENT_LIBRARIES PREPEND "$<TARGET_FILE_DIR:")
    list(TRANSFORM DEPENDENT_LIBRARIES APPEND ">")
    set(_scan_dirs "$<TARGET_FILE_DIR:Qt${PRECESS_QT_MAJOR}::Core>" ${DEPENDENT_LIBRARIES})
    if(TARGET TKernel)
        list(APPEND _scan_dirs "$<TARGET_FILE_DIR:TKernel>")
    endif()

    _precess_plugin_dep_exclude_regexes(plugin_post_exclude_regexes plugin_pre_exclude_regexes)

    install(
        TARGETS ${TARGET}
        RUNTIME_DEPENDENCIES
        DIRECTORIES ${_scan_dirs}
        PRE_EXCLUDE_REGEXES ${plugin_pre_exclude_regexes}
        POST_EXCLUDE_REGEXES ${plugin_post_exclude_regexes}
        # COMPONENT 放 dest 之前（通用绑定）：dest 级关键字按位置归属，放 BUNDLE 后
        # 会把 RUNTIME 产物漏进默认 Unspecified（隐藏恒装）
        COMPONENT AllPlugins
        FRAMEWORK DESTINATION $<IF:$<PLATFORM_ID:Darwin>,${PRECESS_APP_NAME}.app/Contents/Frameworks,Frameworks>
        LIBRARY DESTINATION $<IF:$<PLATFORM_ID:Darwin>,${PRECESS_APP_NAME}.app/Contents/Frameworks,plugins>
        BUNDLE DESTINATION .
    )
endfunction()

# precess_plugin_install(<target> [DESTINATION <dir>])
# 安装动态插件到 PreCess 插件目录并收拢其三方运行时依赖（RUNTIME_DEPENDENCIES）。
# 外部工程必调（默认装入 PRECESS_PLUGIN_INSTALL_DIR，即 PreCess 运行时扫描的 plugins/）；
# in-tree 插件已在装配时自动安装，无需调用。
function(precess_plugin_install TARGET)
    if(NOT TARGET ${TARGET})
        message(FATAL_ERROR "precess_plugin_install: 目标 ${TARGET} 不存在")
    endif()
    cmake_parse_arguments("ppi" "" "DESTINATION" "" ${ARGN})
    if(ppi_DESTINATION)
        set(dest "${ppi_DESTINATION}")
    elseif(DEFINED PRECESS_PLUGIN_INSTALL_DIR)
        set(dest "${PRECESS_PLUGIN_INSTALL_DIR}")
    else()
        message(FATAL_ERROR "precess_plugin_install: 未指定 DESTINATION 且 PRECESS_PLUGIN_INSTALL_DIR 未定义")
    endif()

    get_property(_extra_libs TARGET ${TARGET} PROPERTY PRECESS_PLUGIN_DEP_LIBS)
    set(_dep_dirs "$<TARGET_FILE_DIR:Qt${PRECESS_QT_MAJOR}::Core>")
    if(TARGET TKernel)
        list(APPEND _dep_dirs "$<TARGET_FILE_DIR:TKernel>")
    endif()
    foreach(_lib IN LISTS _extra_libs)
        if(TARGET ${_lib})
            list(APPEND _dep_dirs "$<TARGET_FILE_DIR:${_lib}>")
        endif()
    endforeach()

    _precess_plugin_dep_exclude_regexes(_post_exclude _pre_exclude)

    install(
        TARGETS ${TARGET}
        RUNTIME_DEPENDENCIES
        DIRECTORIES ${_dep_dirs}
        PRE_EXCLUDE_REGEXES ${_pre_exclude}
        POST_EXCLUDE_REGEXES ${_post_exclude}
        # COMPONENT 置于 dest 之前（通用绑定，同 in-tree 分支）：紧随 LIBRARY 会让
        # RUNTIME 收拢产物漏进默认 Unspecified；组件名与 in-tree 统一为 AllPlugins
        COMPONENT AllPlugins
        RUNTIME DESTINATION "${dest}"
        LIBRARY DESTINATION "${dest}"
    )
endfunction()
