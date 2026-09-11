# precess Python 绑定模块（LGPLv3）
#
# pybind11 优先经依赖包提供（CMAKE_PREFIX_PATH），其次本机 pip 安装包
# （python -m pybind11 --cmakedir 定位）；均未命中时配置失败。
# Python 不可达时整目录静默跳过。

set(PRECESS_BUILD_PYTHON ON CACHE BOOL "构建 Python 绑定模块 precess")

find_package(Python3 COMPONENTS Interpreter Development.Module QUIET)
if (NOT PRECESS_BUILD_PYTHON)
    return()
endif()
if (NOT Python3_FOUND)
    message(STATUS "未找到 Python3 解释器/开发库，跳过 precess 绑定模块构建")
    return()
endif()

# pybind11 查找顺序：依赖包提供（CMAKE_PREFIX_PATH）→ 本机 pip 安装
# （python -m pybind11 --cmakedir 定位）；均未命中时配置失败，请先经
# PreCess-deps.py 安装依赖或 pip install pybind11。
find_package(pybind11 CONFIG QUIET)
if (NOT pybind11_FOUND)
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" -m pybind11 --cmakedir
        OUTPUT_VARIABLE _pybind11_dir
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE _pybind11_result
    )
    find_package(pybind11 CONFIG QUIET HINTS "${_pybind11_dir}")
endif()
if (NOT pybind11_FOUND)
    message(FATAL_ERROR
        "未找到 pybind11：请先运行 PreCess-deps.py 安装依赖包，或 pip install pybind11"
    )
endif()

pybind11_add_module(precess src/precess_module.cpp)
target_link_libraries(precess PRIVATE
    Session
)

if (BUILD_TESTING)
    add_test(NAME precess_python_smoke
        COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/test/test_precess.py" --plugins "${CMAKE_BINARY_DIR}/plugins")
    # Python 3.8 起扩展模块的依赖 DLL 不再经 PATH 解析，须经脚本侧
    # os.add_dll_directory 注册：DLL 目录清单经 PRECESS_DLL_DIRS 传递
    set_tests_properties(precess_python_smoke PROPERTIES
        ENVIRONMENT "PYTHONPATH=$<TARGET_FILE_DIR:precess>"
        ENVIRONMENT_MODIFICATION "$<LIST:TRANSFORM,$<TARGET_RUNTIME_DLL_DIRS:precess>,PREPEND,PATH=path_list_prepend:>;$<LIST:TRANSFORM,$<TARGET_RUNTIME_DLL_DIRS:precess>,PREPEND,PRECESS_DLL_DIRS=path_list_append:>")
endif()
