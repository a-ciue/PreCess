add_library(FeatureSystem STATIC FeatureSystem.cpp FeatureSystemRegister.cpp FeatureParams.cpp InteractionContext.cpp)
target_link_libraries(FeatureSystem PRIVATE
    Data
    Qt6::Core
    spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>
)
target_link_libraries(FeatureSystem PUBLIC
    SystemCore Core
)
target_include_directories(FeatureSystem PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}   # 当前文件所在路径
)

add_subdirectory(test)
