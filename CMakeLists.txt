cmake_minimum_required(VERSION 3.17)

set(MYNAME PreCess)

project(${MYNAME} VERSION 0.2.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(INSERT CMAKE_MODULE_PATH 0 "${PROJECT_SOURCE_DIR}/cmake")

option(BUILD_TESTING "是否启用并构建测试用例" OFF)

find_package(spdlog REQUIRED)

find_package(OpenCASCADE REQUIRED)

find_package(VTK REQUIRED COMPONENTS
        CommonColor
        CommonCore
        GUISupportQtQuick
        InteractionStyle
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

find_package("Qt${VTK_QT_VERSION}" REQUIRED COMPONENTS Gui Quick)
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