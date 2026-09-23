# PreCess 插件测试 API（in-tree / 外部 SDK 同源）
#
# in-tree 由 cmake/test.cmake include；外部工程经
# find_package(PreCess COMPONENTS Tests) 由 PreCessConfig.cmake include。
# 测试目标链接插件的 <target>lib 静态库 + PreCess::Base（in-tree 亦可继续用
# 旧模块目标名，如 FeatureSystem、Data）。

include_guard(GLOBAL)

include(CTest)

# precess_add_test(<target> <source1> <source2> ...)
function(precess_add_test TARGET)
    if(BUILD_TESTING)
        add_executable(${ARGV})
        target_link_libraries(${TARGET} PRIVATE Catch2::Catch2WithMain)

        # 测试可执行文件自身目录恒存在，保证下方 ENVIRONMENT_MODIFICATION 不为空；
        # 空值会在属性列表传递中丢失，导致后续属性错位、测试以 BAD_COMMAND 失败
        set(dl_paths "$<TARGET_FILE_DIR:${TARGET}>$<$<BOOL:$<TARGET_RUNTIME_DLL_DIRS:${TARGET}>>:$<SEMICOLON>$<TARGET_RUNTIME_DLL_DIRS:${TARGET}>>")
        set(test_properties)
        if(WIN32)
            # 添加环境变量修改，确保测试运行时能找到所需的DLL
            list(APPEND test_properties
                # 反斜杠多加几个是为了在生成的.cmake文件中能正确解析，因为在最后对生成器表达式求值时需要一个反斜杠给分号转义，中间有各种转义处理会丢失反斜杠
			    ENVIRONMENT_MODIFICATION "$<JOIN:$<LIST:TRANSFORM,${dl_paths},PREPEND,PATH=path_list_prepend:>,\\\\\\\\\\\\\\\\\;>"
            )
        elseif(UNIX)
            # Linux/macOS 下依赖库经 rpath 解析：CMAKE_PREFIX_PATH 的各包目录下取 lib，
            # 保证 VTK/OCCT/Qt 等非系统安装路径下的动态库能被 ctest 找到
            set(test_build_rpath "")
            foreach(_prefix IN LISTS CMAKE_PREFIX_PATH)
                if(EXISTS "${_prefix}/lib")
                    list(APPEND test_build_rpath "${_prefix}/lib")
                endif()
            endforeach()
            # $ORIGIN 与 @loader_path 语义相同：分别面向 Linux 与 macOS 的动态库加载器
            if(APPLE)
                set(test_install_rpath "@loader_path")
            else()
                set(test_install_rpath "$ORIGIN")
            endif()
            set_target_properties(${TARGET} PROPERTIES
                BUILD_RPATH "${test_build_rpath}"
                INSTALL_RPATH "${test_install_rpath}"
            )
        endif()

        catch_discover_tests(${TARGET}
            PROPERTIES ${test_properties}
            DL_PATHS ${dl_paths}
        )
    endif()
endfunction()

# precess_test_link_libraries(<target> <lib1> <lib2> ...)
function(precess_test_link_libraries TARGET)
    if(BUILD_TESTING)
        target_link_libraries(${TARGET} PRIVATE ${ARGN})
    endif()
endfunction()

if(BUILD_TESTING AND NOT TARGET Catch2::Catch2WithMain)
    find_package(Catch2 REQUIRED)
    include(Catch)
endif()
