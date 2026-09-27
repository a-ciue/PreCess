# PreCess find_package 组件解析（命名契约：组件名与其导入目标同名对应）
#
# 组件语义（见 docs/plugin-sdk-design.md §6）：
#   Python   提供 PreCess::Python（仅真实 Python 构建）
#   Tests    插件测试 API（工具组件，不产生目标）
include_guard(GLOBAL)

set(_PRECESS_MODULES_DIR "${CMAKE_CURRENT_LIST_DIR}")

function(precess_modules_resolve_components)
    foreach(_comp IN LISTS PreCess_FIND_COMPONENTS)
        set(_found FALSE)
        if(_comp STREQUAL "Tests")
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
