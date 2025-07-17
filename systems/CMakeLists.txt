add_subdirectory(io)

add_library(Systems STATIC "SystemPluginManager.cpp")
target_link_libraries(Systems INTERFACE IOSystem)
target_link_libraries(Systems PRIVATE 
	spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>
	Qt6::Core
)