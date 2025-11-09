# precess_add_test(<target> <source1> <source2> ...)
function(precess_add_test TARGET)
    if(BUILD_TESTING)
        add_executable(${ARGV})
        target_link_libraries(${TARGET} PRIVATE Catch2::Catch2WithMain)

        set(test_properties)
        if(WIN32)
            list(APPEND test_properties
                # 反斜杠多加几个是为了在生成的.cmake文件中能正确解析，因为在最后对生成器表达式求值时需要一个反斜杠给分号转义，中间有各种转义处理会丢失反斜杠
			    ENVIRONMENT_MODIFICATION "$<JOIN:$<LIST:TRANSFORM,$<TARGET_RUNTIME_DLL_DIRS:${TARGET}>,PREPEND,PATH=path_list_prepend:>,\\\\\\\\\\\\\\\\\;>"
            )            
        endif()

        catch_discover_tests(${TARGET}
            PROPERTIES ${test_properties}
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
