precess_add_test(TestCreateLineFromVerticesPlugin TestCreateLineFromVerticesPlugin.cpp)
precess_test_link_libraries(TestCreateLineFromVerticesPlugin CreateLineFromVerticesPluginlib Systems FeatureSystem Data Qt6::Core)

if(BUILD_TESTING)
    # 被测插件 dll 的路径注入测试，且测试运行前需先构建插件
    target_compile_definitions(TestCreateLineFromVerticesPlugin PRIVATE CREATE_LINE_FROM_VERTICES_PLUGIN_PATH="$<TARGET_FILE:CreateLineFromVerticesPlugin>")
    add_dependencies(TestCreateLineFromVerticesPlugin CreateLineFromVerticesPlugin)
endif()
