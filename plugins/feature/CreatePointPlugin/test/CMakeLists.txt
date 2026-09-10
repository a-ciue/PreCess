precess_add_test(TestCreatePointPlugin TestCreatePointPlugin.cpp)
precess_test_link_libraries(TestCreatePointPlugin CreatePointPluginlib Systems FeatureSystem Data Qt6::Core)

if(BUILD_TESTING)
    # 被测插件 dll 的路径注入测试，且测试运行前需先构建插件
    target_compile_definitions(TestCreatePointPlugin PRIVATE CREATE_POINT_PLUGIN_PATH="$<TARGET_FILE:CreatePointPlugin>")
    add_dependencies(TestCreatePointPlugin CreatePointPlugin)
endif()
