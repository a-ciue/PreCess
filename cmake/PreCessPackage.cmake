# PreCess SDK 打包：开发文件安装与 CMake 包生成（find_package(PreCess) 支持）
#
# 安装布局（COMPONENT Development）：
#   include/precess/...            开发头文件（镜像源码模块布局，裸名 include 语义不变）
#   lib/PreCessBase.lib（Debug: PreCessBased.lib）  模型层单一静态库（导出 PreCess::Base）
#   lib/cmake/PreCess/             PreCessConfig/Targets/Plugin/PluginTesting/ABI/
#                                  find-package-helpers/Modules + Findtetgen/Findfreetype
include_guard(GLOBAL)

option(PRECESS_INSTALL_DEVELOPMENT_FILES
    "安装插件 SDK 开发文件（头文件、模型层库、CMake 包配置），供 find_package(PreCess) 使用" ON)
option(PRECESS_RELOCATABLE_INSTALL
    "可重定位安装：不安装含绝对路径的三方包查找引导文件（PreCess-find-package-helpers）" OFF)

if(NOT PRECESS_INSTALL_DEVELOPMENT_FILES)
    return()
endif()

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

set(PRECESS_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/PreCess")

# ---- 1) 导出目标 -------------------------------------------------------------
install(TARGETS PreCessBase EXPORT PreCessTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    COMPONENT Development
)
# 内嵌 Python 宿主（可选组件 Python 对应 PreCess::Python）
if(TARGET precess_bindings)
    set(PRECESS_WITH_PYTHON ON)
else()
    set(PRECESS_WITH_PYTHON OFF)
endif()
if(PRECESS_WITH_PYTHON)
    install(TARGETS precess_runtime EXPORT PreCessTargets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        COMPONENT Development
    )
endif()

install(EXPORT PreCessTargets
    NAMESPACE PreCess::
    DESTINATION "${PRECESS_INSTALL_CMAKEDIR}"
    COMPONENT Development
)

# ---- 2) 开发头文件（按源码根目录镜像安装）---------------------------------------
# include/precess/<模块路径> 布局与 model/CMakeLists.txt 的模块清单一致；
# 快照式递归安装两个根即覆盖全部公共头，各级 test/ 目录排除（细粒度模块清单
# 只在 model/CMakeLists.txt 维护一处，此处不重复）。
foreach(_root IN ITEMS core model)
    install(DIRECTORY "${PROJECT_SOURCE_DIR}/${_root}/"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/precess/${_root}"
        COMPONENT Development
        FILES_MATCHING PATTERN "*.h"
        PATTERN "test" EXCLUDE
    )
endforeach()
if(PRECESS_WITH_PYTHON)
    install(DIRECTORY "${PROJECT_SOURCE_DIR}/python/include/"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/precess/python"
        COMPONENT Development
        FILES_MATCHING PATTERN "*.h"
    )
endif()

# ---- 3) CMake 包配置 ---------------------------------------------------------
configure_package_config_file(cmake/PreCessConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/PreCessConfig.cmake"
    INSTALL_DESTINATION "${PRECESS_INSTALL_CMAKEDIR}"
)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/PreCessConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMinorVersion
)
configure_file(cmake/PreCessABI.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/PreCessABI.cmake" @ONLY)

set(_precess_config_files
    "${CMAKE_CURRENT_BINARY_DIR}/PreCessConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/PreCessConfigVersion.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/PreCessABI.cmake"
    "${CMAKE_SOURCE_DIR}/cmake/PreCessPlugin.cmake"
    "${CMAKE_SOURCE_DIR}/cmake/PreCessPluginTesting.cmake"
    "${CMAKE_SOURCE_DIR}/cmake/PreCessModules.cmake"
    "${CMAKE_SOURCE_DIR}/cmake/Findfreetype.cmake"
)
if(NOT PRECESS_RELOCATABLE_INSTALL)
    # 三方包查找引导含构建机绝对路径，可重定位发行包不安装（同 ParaView 的
    # PARAVIEW_RELOCATABLE_INSTALL 取舍）
    configure_file(cmake/PreCess-find-package-helpers.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/PreCess-find-package-helpers.cmake" @ONLY)
    list(APPEND _precess_config_files
        "${CMAKE_CURRENT_BINARY_DIR}/PreCess-find-package-helpers.cmake")
endif()

install(FILES ${_precess_config_files}
    DESTINATION "${PRECESS_INSTALL_CMAKEDIR}"
    COMPONENT Development
)
