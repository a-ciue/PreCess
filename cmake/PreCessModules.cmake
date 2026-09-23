# PreCess find_package 组件解析
#
# 组件语义（见 docs/plugin-sdk-design.md §6）：
#   IO / Algo / Edit / Feature  系统接口声明：校验对应接口头随包存在，
#                               全部映射到同一目标 PreCess::Base
#   Tests                       加载插件测试 API（PreCessPluginTesting.cmake）
#   Python                      提供 PreCess::Runtime（仅真实 Python 构建）
include_guard(GLOBAL)

set(_PRECESS_MODULES_DIR "${CMAKE_CURRENT_LIST_DIR}")

function(precess_modules_resolve_components)
    get_target_property(_precess_includes PreCess::Base INTERFACE_INCLUDE_DIRECTORIES)
    foreach(_comp IN LISTS PreCess_FIND_COMPONENTS)
        set(_found FALSE)
        set(_hdr "")
        if(_comp STREQUAL "IO")
            set(_hdr "ModelIOHandler.h")
        elseif(_comp STREQUAL "Algo")
            set(_hdr "AlgorithmHandler.h")
        elseif(_comp STREQUAL "Edit")
            set(_hdr "EditHandler.h")
        elseif(_comp STREQUAL "Feature")
            set(_hdr "FeatureHandler.h")
        endif()

        if(_hdr)
            # 系统接口声明：接口头随 PreCess::Base 的 include 目录提供
            set(_found FALSE)
            foreach(_inc IN LISTS _precess_includes)
                if(EXISTS "${_inc}/${_hdr}")
                    set(_found TRUE)
                    break()
                endif()
            endforeach()
            if(NOT _found)
                message(WARNING "PreCess 组件 ${_comp} 的接口头文件 ${_hdr} 缺失（安装不完整？）")
            endif()
        elseif(_comp STREQUAL "Tests")
            include("${_PRECESS_MODULES_DIR}/PreCessPluginTesting.cmake")
            set(_found TRUE)
        elseif(_comp STREQUAL "Python")
            if(PRECESS_USE_PYTHON)
                find_package(pybind11 CONFIG QUIET)
                if(pybind11_FOUND)
                    set(_found TRUE)
                else()
                    message(WARNING "PreCess 组件 Python 需要 pybind11：未找到，组件不可用")
                endif()
            else()
                message(WARNING "此 PreCess 构建不含 Python 宿主（PRECESS_BUILD_PYTHON=OFF 或依赖缺失）")
            endif()
        else()
            message(WARNING "未知的 PreCess 组件：${_comp}")
        endif()
        set(PreCess_${_comp}_FOUND ${_found} PARENT_SCOPE)
    endforeach()
endfunction()
