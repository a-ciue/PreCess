add_library(IOSystem STATIC ModelIOSystem.cpp ModelIOHandler.cpp "OBJModelHandler.cpp")
target_link_libraries(IOSystem PRIVATE 
	spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>
	${OpenCASCADE_LIBRARIES}
)
