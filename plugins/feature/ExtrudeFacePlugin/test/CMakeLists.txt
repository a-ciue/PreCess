precess_add_test(TestExtrudeFacePlugin TestExtrudeFacePlugin.cpp)
precess_test_link_libraries(TestExtrudeFacePlugin ExtrudeFacePluginlib Systems FeatureSystem Data Qt6::Core)

if(BUILD_TESTING)
    # 被测插件 dll 的路径注入测试，且测试运行前需先构建插件
    target_compile_definitions(TestExtrudeFacePlugin PRIVATE EXTRUDE_FACE_PLUGIN_PATH="$<TARGET_FILE:ExtrudeFacePlugin>")
    add_dependencies(TestExtrudeFacePlugin ExtrudeFacePlugin)
endif()
