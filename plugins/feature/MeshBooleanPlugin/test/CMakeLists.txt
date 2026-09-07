precess_add_test(TestMeshBooleanPlugin TestMeshBooleanPlugin.cpp)
precess_test_link_libraries(
    TestMeshBooleanPlugin
    MeshBooleanPluginlib
    FeatureSystem
    Data
)
