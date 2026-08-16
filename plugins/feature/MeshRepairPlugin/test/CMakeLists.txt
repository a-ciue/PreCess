precess_add_test(TestMeshRepairPlugin TestMeshRepairPlugin.cpp)
precess_test_link_libraries(
    TestMeshRepairPlugin
    MeshRepairPluginlib
    FeatureSystem
    Data
)