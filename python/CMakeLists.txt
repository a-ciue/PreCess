# precess Python 绑定模块与内嵌解释器宿主（LGPLv3）
#
# 产物：
# - precess 模块（pyd）：会话层绑定，需 Python3(Interpreter/Module) + pybind11
# - precess_runtime 静态库（无 Qt）：内嵌解释器宿主，目标恒存在——Python3 +
#   pybind11 可用且非 wasm 时编译 runtime.cpp（真实现），否则编译 stub.cpp
#   （桩：available 恒 false、错误信息说明原因），宿主（GUI/CLI）无需条件编译


set(_precess_real OFF)
if(PRECESS_BUILD_PYTHON AND Python3_FOUND AND NOT EMSCRIPTEN
        AND Python3_Development.Module_FOUND AND Python3_Development.Embed_FOUND
        AND pybind11_FOUND)
    set(_precess_real ON)
elseif(PRECESS_BUILD_PYTHON AND NOT EMSCRIPTEN)
    if(NOT Python3_Interpreter_FOUND)
        message(WARNING
            "请确认 Python 被正确加入环境变量中，或者手动指定解释器所在目录 PRECESS_PYTHON_PATH=\"${PRECESS_PYTHON_PATH}\"")
    elseif(NOT pybind11_FOUND)
        message(STATUS
            "未找到 pybind11，precess_runtime 降级为桩模式（运行时 Python 功能不可用）；"
            "安装请经 PreCess-deps.py 或 pip install pybind11")
    endif()
endif()

# 导出构建所用解释器目录（标准库根/解释器运行库所在），运行时宿主配置
# 与 app 安装依赖收拢共用（跨目录可见的缓存变量；桩模式下为空）
if(_precess_real)
    get_filename_component(PRECESS_PYTHON_HOME_DIR "${Python3_EXECUTABLE}" DIRECTORY)
else()
    set(PRECESS_PYTHON_HOME_DIR "")
endif()
# FORCE：set(CACHE) 对已存在条目默认不覆盖——依赖发现失败的某次配置写入
# 空值/旧值后会永久粘住（含 CMake 自动 re-configure 的第二遍），必须可写回
set(PRECESS_PYTHON_HOME_DIR "${PRECESS_PYTHON_HOME_DIR}" CACHE INTERNAL
    "构建所用 Python 解释器目录（标准库根；桩模式为空）" FORCE)

# 内嵌解释器宿主（静态库，无 Qt）：目标恒存在，真实现/桩按可用性二选一
add_library(precess_runtime STATIC)
target_include_directories(precess_runtime PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(precess_runtime PUBLIC Session)
if(_precess_real)
    target_sources(precess_runtime PRIVATE runtime.cpp)
    target_link_libraries(precess_runtime PRIVATE pybind11::embed spdlog::spdlog)
    # 宿主配置宏随库 PUBLIC 传递：消费方（app/model 的 QPythonRuntime、测试）
    # 直接可用，无需各自按模式重复判断；桩模式由 QPythonRuntime.cpp 的 #ifndef
    # 空值兜底（桩忽略 config）
    target_compile_definitions(precess_runtime PUBLIC
        # 嵌入式解释器的标准库根：构建机解释器目录（运行期另作捆绑 Lib 探测，
        # 见 QPythonRuntime::makeRuntimeConfig）
        PRECESS_PYTHON_HOME="${PRECESS_PYTHON_HOME_DIR}"
        # 构建树中 precess 扩展模块输出目录（运行时另一候选为 <exe_dir>/python）
        PRECESS_PYTHON_MODULE_DIR="$<TARGET_FILE_DIR:precess_bindings>"
    )
else()
    target_sources(precess_runtime PRIVATE stub.cpp)
endif()

if(_precess_real)
    # CMake 目标名取 precess_bindings 与主程序 PreCess 区分；Python 导入名
    # 保持 precess 不变（经 OUTPUT_NAME 固定输出文件名）
    pybind11_add_module(precess_bindings precess_module.cpp)
    set_target_properties(precess_bindings PROPERTIES OUTPUT_NAME "precess")
    target_link_libraries(precess_bindings PRIVATE
        Session
    )

    # 构建树自包含（仅 Windows）：把解释器运行库（版本相关，经导入目标求值）
    # 拷到可执行文件目录（Qt 工程运行时产物统一在构建根目录），运行/调试不再
    # 依赖 PATH 中的 Python；类 Unix 由 rpath 解析 libpython，无需拷贝
    if(WIN32)
        add_custom_command(TARGET precess_bindings POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:Python3::Python>"
                "${CMAKE_BINARY_DIR}"
        )
    endif()

    if (BUILD_TESTING)
        add_test(NAME precess_python_smoke
            COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/test/test_precess.py" --plugins "${CMAKE_BINARY_DIR}/plugins")
        # Python 3.8 起扩展模块的依赖 DLL 不再经 PATH 解析，须经脚本侧
        # os.add_dll_directory 注册：DLL 目录清单经 PRECESS_DLL_DIRS 传递
        set_tests_properties(precess_python_smoke PROPERTIES
            ENVIRONMENT "PYTHONPATH=$<TARGET_FILE_DIR:precess_bindings>"
            ENVIRONMENT_MODIFICATION "$<LIST:TRANSFORM,$<TARGET_RUNTIME_DLL_DIRS:precess_bindings>,PREPEND,PATH=path_list_prepend:>;$<LIST:TRANSFORM,$<TARGET_RUNTIME_DLL_DIRS:precess_bindings>,PREPEND,PRECESS_DLL_DIRS=path_list_append:>")
        # C++ 嵌入测试：验证宿主初始化、控制台执行与活会话调用（依赖真实
        # 宿主与 precess 模块，桩模式无意义）；宿主配置宏经 precess_runtime
        # PUBLIC 传递，此处仅补测试专属的插件目录
        precess_add_test(TestPrecessRuntime test/test_runtime.cpp)
        target_link_libraries(TestPrecessRuntime PRIVATE precess_runtime)
        target_compile_definitions(TestPrecessRuntime PRIVATE
            PRECESS_PLUGIN_DIR="${CMAKE_BINARY_DIR}/plugins")
    endif()

    # 安装树自包含：pyd 装到宿主首选模块目录 <exe_dir>/python，解释器标准库
    # 与二进制扩展随包分发到 exe 目录（运行库本身由 app 依赖收拢放入）；
    # 排除开发/测试内容控制体积，非标准布局（无 Lib/）时跳过
    install(TARGETS precess_bindings
        LIBRARY DESTINATION "${CMAKE_INSTALL_BINDIR}/python"
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}/python"
    )
    if(EXISTS "${PRECESS_PYTHON_HOME_DIR}/Lib")
        install(DIRECTORY "${PRECESS_PYTHON_HOME_DIR}/Lib"
            DESTINATION "${CMAKE_INSTALL_BINDIR}"
            PATTERN "__pycache__" EXCLUDE
            PATTERN "site-packages" EXCLUDE
            PATTERN "idlelib" EXCLUDE
            PATTERN "tkinter" EXCLUDE
            PATTERN "tcl" EXCLUDE
            PATTERN "test" EXCLUDE
            PATTERN "tests" EXCLUDE
        )
        if(EXISTS "${PRECESS_PYTHON_HOME_DIR}/DLLs")
            install(DIRECTORY "${PRECESS_PYTHON_HOME_DIR}/DLLs"
                DESTINATION "${CMAKE_INSTALL_BINDIR}"
                PATTERN "__pycache__" EXCLUDE
            )
        endif()
    else()
        message(STATUS
            "解释器目录 ${PRECESS_PYTHON_HOME_DIR} 下无标准库 Lib/，安装树将不随包分发 Python 标准库（依赖目标机器的 Python 安装）")
    endif()
endif()
