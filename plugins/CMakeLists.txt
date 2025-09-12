add_custom_target(AllPlugins)
set(PLUGINS_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}")

function(precess_add_io_plugin)
	set(TARGET ${ARGV0})
	qt_add_plugin(${ARGV})
	target_link_libraries(${TARGET} PRIVATE
		Core Data
		$<COMPILE_ONLY:IOSystem>
		Qt6::Core
		spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>
	)
	set_target_properties(${TARGET} PROPERTIES
		LIBRARY_OUTPUT_DIRECTORY "${PLUGINS_BUILD_DIR}"
	)
	target_include_directories(${TARGET} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
	add_dependencies(AllPlugins ${TARGET})

	install(TARGETS ${TARGET}
		LIBRARY DESTINATION "./plugins"
        COMPONENT AllPlugins
	)
endfunction()

function(precess_add_algo_plugin)
	set(TARGET ${ARGV0})
	qt_add_plugin(${ARGV})
	target_link_libraries(${TARGET} PRIVATE
		Core Data AlgorithmSystem
		$<COMPILE_ONLY:IOSystem>
		Qt6::Core
		spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>
	)
	set_target_properties(${TARGET} PROPERTIES
		LIBRARY_OUTPUT_DIRECTORY "${PLUGINS_BUILD_DIR}"
	)
	target_include_directories(${TARGET} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
	add_dependencies(AllPlugins ${TARGET})

	install(TARGETS ${TARGET}
		LIBRARY DESTINATION "./plugins"
        COMPONENT AllPlugins
	)
endfunction()

add_subdirectory(io)
add_subdirectory(algo)