precess_add_test(TestCreateFaceFromEdgesPlugin TestCreateFaceFromEdgesPlugin.cpp)
precess_test_link_libraries(TestCreateFaceFromEdgesPlugin CreateFaceFromEdgesPluginlib Systems FeatureSystem Data Qt6::Core)

if(BUILD_TESTING)
    # 被测插件 dll 的路径注入测试，且测试运行前需先构建插件
    target_compile_definitions(TestCreateFaceFromEdgesPlugin PRIVATE CREATE_FACE_FROM_EDGES_PLUGIN_PATH="$<TARGET_FILE:CreateFaceFromEdgesPlugin>")
    add_dependencies(TestCreateFaceFromEdgesPlugin CreateFaceFromEdgesPlugin)
endif()
