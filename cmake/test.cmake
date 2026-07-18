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

include(CTest)
if(BUILD_TESTING)
    find_package(Catch2 REQUIRED)
    include(Catch)
endif()
