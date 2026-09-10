precess_add_test(TestDeleteGeometryPlugin TestDeleteGeometryPlugin.cpp)
precess_test_link_libraries(TestDeleteGeometryPlugin DeleteGeometryPluginlib Systems FeatureSystem Data Qt6::Core)

if(BUILD_TESTING)
    # 被测插件 dll 的路径注入测试，且测试运行前需先构建插件
    target_compile_definitions(TestDeleteGeometryPlugin PRIVATE DELETE_GEOMETRY_PLUGIN_PATH="$<TARGET_FILE:DeleteGeometryPlugin>")
    add_dependencies(TestDeleteGeometryPlugin DeleteGeometryPlugin)
endif()
