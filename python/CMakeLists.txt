# precess Python 绑定模块（LGPLv3）
#
# pybind11 优先经 pip 安装包提供（python -m pybind11 --cmakedir 定位），
# 未安装时回退 FetchContent 拉取；Python 不可达时整目录静默跳过。

set(PRECESS_BUILD_PYTHON ON CACHE BOOL "构建 Python 绑定模块 precess")

find_package(Python3 COMPONENTS Interpreter Development.Module QUIET)
if (NOT PRECESS_BUILD_PYTHON)
    return()
endif()
if (NOT Python3_FOUND)
    message(STATUS "未找到 Python3 解释器/开发库，跳过 precess 绑定模块构建")
    return()
endif()

# pip 安装的 pybind11 不在默认搜索路径，经模块自带命令定位其 CMake 包目录
execute_process(
    COMMAND "${Python3_EXECUTABLE}" -m pybind11 --cmakedir
    OUTPUT_VARIABLE _pybind11_dir
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE _pybind11_result
)
find_package(pybind11 CONFIG QUIET
    HINTS "${_pybind11_dir}"
)
if (NOT pybind11_FOUND)
    message(STATUS "未找到已安装的 pybind11，回退 FetchContent 拉取（需要网络）")
    include(FetchContent)
    FetchContent_Declare(pybind11
        GIT_REPOSITORY https://github.com/pybind11/pybind11.git
        GIT_TAG v3.1.0
    )
    FetchContent_MakeAvailable(pybind11)
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
