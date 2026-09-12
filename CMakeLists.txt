cmake_minimum_required(VERSION 3.17)

set(MYNAME PreCess)

project(${MYNAME} VERSION 0.4.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 项目以异常作错误处理机制：MSVC 缺省（无 /EH）下 catch 能捕获但不展开栈（C4530），
# 捕获异常后继续执行会状态错乱（如 unordered_set 迭代器损坏），须显式启用 /EHsc
# 源码统一 UTF-8 无 BOM：MSVC 在代码页 936 下按 GBK 误读会报 C4819，须显式指定源字符集；
# Qt6（Qt6Targets.cmake）与 VTK::fmt 已对 MSVC 目标强制传播 -utf-8，
# 与 /source-charset:utf-8 不兼容（D8016），故 MSVC 统一用 /utf-8
if(MSVC)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/EHsc> $<$<COMPILE_LANGUAGE:CXX>:/utf-8>)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-finput-charset=UTF-8>)
endif()

if(UNIX)
    # 链接到动态库的静态库必须启用 PIC
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    add_link_options($<$<PLATFORM_ID:Linux>:-Wl,--disable-new-dtags>)
endif()

if(APPLE)
    # Apple libc++ 的 std::execution 并行算法属实验特性，须显式开启
    add_compile_options(-fexperimental-library)
    # 构建时注入 rpath 用于调试和测试，安装时的 rpath 由 fixup_bundle 处理
    foreach(_prefix IN LISTS CMAKE_PREFIX_PATH)
        if(EXISTS "${_prefix}/lib")
            list(APPEND CMAKE_BUILD_RPATH "${_prefix}/lib")
        endif()
    endforeach()
endif()

if(EMSCRIPTEN)
    # for std::execution
    add_compile_options(-fexperimental-library)
    # VTK 9.6 use wasm exceptions
    add_compile_options(-fwasm-exceptions)
    # libMeshb depends on setjmp/longjmp
    add_link_options(-sSUPPORT_LONGJMP=wasm)
    option(WASM_THREADS "启用 WebAssembly 多线程支持" OFF)
    if(WASM_THREADS)
        add_compile_options(-pthread)
        add_link_options(-pthread)
    endif()
    # 让 VTK 使用 Qt 内置的 freetype/tiff，避免静态链接报符号重定义的错误
    get_filename_component(WASM_QT_DIR "${CMAKE_TOOLCHAIN_FILE}/../../../.." ABSOLUTE)
    get_filename_component(WASM_SOURCE_DIR "${WASM_QT_DIR}/../../_source" ABSOLUTE)
    set(TIFF_LIBRARY "${WASM_QT_DIR}/plugins/imageformats/libqtiff.a" CACHE FILEPATH "" FORCE)
    set(TIFF_INCLUDE_DIR "${WASM_SOURCE_DIR}/qt5/qtimageformats/src/3rdparty/libtiff/libtiff" CACHE PATH "" FORCE)
    set(FREETYPE_INCLUDE_DIR_ft2build "${WASM_QT_DIR}/include/QtFreetype" CACHE PATH "" FORCE)
    set(FREETYPE_INCLUDE_DIR_freetype2 "${WASM_QT_DIR}/include/QtFreetype" CACHE PATH "" FORCE)
    set(FREETYPE_LIBRARY "${WASM_QT_DIR}/lib/libQt6BundledFreetype.a" CACHE FILEPATH "" FORCE)
endif()

option(BUILD_TESTING "是否启用并构建测试用例" OFF)

list(INSERT CMAKE_MODULE_PATH 0 "${PROJECT_SOURCE_DIR}/cmake")

find_package(spdlog REQUIRED)

find_package(OpenCASCADE REQUIRED)

find_package(VTK REQUIRED COMPONENTS
        CommonColor
        CommonCore
        GUISupportQtQuick
        InteractionStyle
        FiltersVerdict
        RenderingAnnotation
)

find_package(freetype CONFIG QUIET)
if(NOT freetype_FOUND)
    message(STATUS "freetype not found in config mode. Trying module mode...")
    find_package(freetype MODULE QUIET)
    if(NOT freetype_FOUND)
        message(WARNING "freetype not found as module. You can set \"freetype_DIR\" to find freetype in module mode.")
        message(WARNING "freetype not found, assuming that maybe freetype is installed along with OpenCASCADE.")
    endif()
endif()

find_package("Qt${VTK_QT_VERSION}" REQUIRED COMPONENTS Gui Quick QuickControls2 Svg)
message(STATUS "QT_VERSION: Qt${VTK_QT_VERSION}")
qt_policy(SET QTP0001 NEW)
qt_standard_project_setup()
include(GNUInstallDirs)
include(test)

add_subdirectory(core)
add_subdirectory(model)
add_subdirectory(plugins)

add_subdirectory(app)
add_subdirectory(resource)

set(CPACK_PACKAGE_NAME ${MYNAME})
set(CPACK_PACKAGE_VENDOR ${MYNAME})
set(CPACK_PACKAGE_ICON "${CMAKE_SOURCE_DIR}\\resource\\PreCess.ico")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "PreCess，专注网格处理的CAE前处理软件。")
set(CPACK_PACKAGE_INSTALL_DIRECTORY ${CPACK_PACKAGE_NAME})
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_VERBATIM_VARIABLES YES)
#set(CPACK_PACKAGE_DESCRIPTION_FILE
#    ${CMAKE_CURRENT_LIST_DIR}/Description.txt
#)
#set(CPACK_RESOURCE_FILE_WELCOME
#    ${CMAKE_CURRENT_LIST_DIR}/Welcome.txt
#)
set(CPACK_RESOURCE_FILE_LICENSE
    ${CMAKE_CURRENT_LIST_DIR}/LICENSE.md
)
set(CPACK_RESOURCE_FILE_README
    ${CMAKE_CURRENT_LIST_DIR}/README.md
)

if(WIN32)
	set(CPACK_GENERATOR ZIP "NSIS") # this needs NSIS installed, and available
elseif (APPLE)
	set(CPACK_GENERATOR TGZ "DragNDrop")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CPACK_GENERATOR TGZ RPM)
else()
	set(CPACK_GENERATOR TGZ)
endif()

# NSIS-specific configuration
set(CPACK_NSIS_MUI_ICON
    ${CPACK_PACKAGE_ICON}
)
set(CPACK_NSIS_MUI_UNIICON
    ${CPACK_PACKAGE_ICON}
)
set(CPACK_NSIS_INSTALLED_ICON_NAME bin/${MYNAME}.exe)
set(CPACK_NSIS_DISPLAY_NAME "${MYNAME}")
set(CPACK_NSIS_PACKAGE_NAME "${MYNAME}")
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL YES)
set(CPACK_NSIS_CREATE_ICONS_EXTRA
    "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\${MYNAME}.lnk' '$INSTDIR\\\\bin\\\\${MYNAME}.exe'
    CreateShortCut '$DESKTOP\\\\${MYNAME}.lnk' '$INSTDIR\\\\bin\\\\${MYNAME}.exe'"
)
set(CPACK_NSIS_DELETE_ICONS_EXTRA
    "Delete '$SMPROGRAMS\\\\$START_MENU\\\\${MYNAME}.lnk'
    Delete '$DESKTOP\\\\${MYNAME}.lnk'"
)

set(SUPPORTED_MESH_EXTS .mesh .meshb .obj .step .stp .iges .igs .vtk .m .ply )
set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS "")
set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS "")

# 注册PreCess到打开方式
string(APPEND CPACK_NSIS_EXTRA_INSTALL_COMMANDS "
WriteRegStr HKCR 'Applications\\${MYNAME}.exe' '' 'PreCess'
WriteRegStr HKCR 'Applications\\${MYNAME}.exe' 'FriendlyAppName' 'PreCess'
WriteRegStr HKCR 'Applications\\${MYNAME}.exe\\shell\\open\\command' '' '\"$INSTDIR\\bin\\${MYNAME}.exe\" \"%1\"'
WriteRegStr HKCR 'Applications\\${MYNAME}.exe\\DefaultIcon' '' '$INSTDIR\\bin\\${MYNAME}.exe,0'
")
string(APPEND CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS "
DeleteRegKey HKCR 'Applications\\${MYNAME}.exe'
")

foreach(ext ${SUPPORTED_MESH_EXTS})
    # 添加右键直接打开菜单
    string(APPEND CPACK_NSIS_EXTRA_INSTALL_COMMANDS "
    WriteRegStr HKCR 'SystemFileAssociations\\${ext}\\shell\\PreCess' '' '使用 PreCess 打开'
    WriteRegStr HKCR 'SystemFileAssociations\\${ext}\\shell\\PreCess\\command' '' '\"$INSTDIR\\bin\\${MYNAME}.exe\" \"%1\"'
    # 添加到打开方式列表
    WriteRegStr HKCR 'SystemFileAssociations\\${ext}\\OpenWithProgids' '${MYNAME}${ext}' ''
    ")
    string(APPEND CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS "
    DeleteRegKey HKCR 'SystemFileAssociations\\${ext}\\shell\\PreCess'
    DeleteRegValue HKCR 'SystemFileAssociations\\${ext}\\OpenWithProgids' '${MYNAME}${ext}'
    ")
endforeach()

include(CPack)

cpack_add_component(${MYNAME}
    DISPLAY_NAME ${MYNAME}
    DESCRIPTION "主程序"
    REQUIRED
    INSTALL_TYPES Full Minimal
)

cpack_add_component(AllPlugins
    DISPLAY_NAME "${MYNAME} 插件"
    DESCRIPTION "所有插件"
    DEPENDS ${MYNAME}
    GROUP Plugins
    INSTALL_TYPES Full
)
cpack_add_component_group(Plugins
    DISPLAY_NAME "功能插件"
    DESCRIPTION "IO与算法功能。"
)

cpack_add_install_type(Full)
cpack_add_install_type(Minimal)

# 将版本号写入一个独立的文件，便于CI读取
file(WRITE "${CMAKE_BINARY_DIR}/project_version.txt" "${PROJECT_VERSION}")
