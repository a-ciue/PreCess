# PreCess 插件测试 API（in-tree / 外部 SDK 同源）
#
# in-tree 由顶层 CMakeLists.txt 直接 include；外部工程经
# find_package(PreCess COMPONENTS Tests) 由 PreCessConfig.cmake include。
# 测试链接写法保持与模型层归并前一致（模块目标名 + 裸 TK*，或插件的 <target>lib +
# FeatureSystem Data）；归并后模块为 OBJECT 库、模块间 link 不传递对象文件，下方
# precess_test_link_libraries 统一追加聚合归档（in-tree PreCessBase / 外部 PreCess::Base）
# 兜底跨模块对象缺口——等价于归并前 STATIC 的传递归档语义，测试文件因此零特判。
# 运行时 DLL 目录由 $<TARGET_RUNTIME_DLL_DIRS> 按链接闭包自动收集，不依赖调用方显式罗列库清单。

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
# 除传入的库外统一追加模型层聚合归档：模块 OBJECT 库之间 link 只传用法需求、
# 不传对象文件，重构前的链接写法（如 TestSession Session TKPrim...、
# TestGeometryBuilder DataOps TK...）依赖的跨模块对象由该归档补足
function(precess_test_link_libraries TARGET)
    if(BUILD_TESTING)
        if(TARGET PreCessBase)
            set(_precess_test_base PreCessBase)
        elseif(TARGET PreCess::Base)
            set(_precess_test_base PreCess::Base)
        else()
            set(_precess_test_base "")
        endif()
        target_link_libraries(${TARGET} PRIVATE ${ARGN} ${_precess_test_base})
    endif()
endfunction()

if(BUILD_TESTING AND NOT TARGET Catch2::Catch2WithMain)
    find_package(Catch2 REQUIRED)
    include(Catch)
endif()
