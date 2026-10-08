precess_add_test(TestSessionQuery TestSessionQuery.cpp)
precess_test_link_libraries(TestSessionQuery Session TKPrim TKBRep TKTopAlgo)

precess_add_test(TestSession TestSession.cpp)
precess_test_link_libraries(TestSession Session TKPrim TKBRep TKTopAlgo)

if (BUILD_TESTING)
    # 测试 DLL 独立输出，不加入产品插件安装集合。
    qt_add_plugin(SetupNavigationTestPlugin
        CLASS_NAME SetupNavigationTestPlugin
        SetupNavigationTestPlugin.cpp)
    target_link_libraries(SetupNavigationTestPlugin PRIVATE PreCessBase Qt6::Core)
    set_target_properties(SetupNavigationTestPlugin PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/plugins"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/plugins")
    precess_add_test(TestSessionPlugins TestSessionPlugins.cpp)
    precess_test_link_libraries(TestSessionPlugins Session TKPrim TKBRep TKTopAlgo Qt6::Core)
    target_compile_definitions(TestSessionPlugins PRIVATE
        PRECESS_PLUGIN_DIR="${CMAKE_BINARY_DIR}/plugins"
        PRECESS_SETUP_TEST_PLUGIN="$<TARGET_FILE:SetupNavigationTestPlugin>")
    add_dependencies(TestSessionPlugins SetupNavigationTestPlugin)
endif()

precess_add_test(TestSessionJobs TestSessionJobs.cpp)
precess_test_link_libraries(TestSessionJobs Session)
