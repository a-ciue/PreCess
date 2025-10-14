add_custom_target(AllPlugins)
set(PLUGINS_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}")

function(precess_add_io_plugin)
	set(TARGET ${ARGV0})
	qt_add_plugin(${ARGV})
	target_link_libraries(${TARGET} PRIVATE
		Core Data DataOps
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
		LIBRARY DESTINATION plugins
        COMPONENT AllPlugins
	)
endfunction()

function(precess_add_algo_plugin)
	set(TARGET ${ARGV0})
	qt_add_plugin(${ARGV})
	target_link_libraries(${TARGET} PRIVATE
		Core Data DataOps AlgorithmSystem
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
		LIBRARY DESTINATION plugins
        COMPONENT AllPlugins
	)
endfunction()

function(precess_plugin_link_libraries)
    if(ARGC LESS_EQUAL 1)
        message(FATAL_ERROR "Function my_function requires at least 2 arguments, but got ${ARGC}")
    endif()

	set(TARGET ${ARGV0})
	list(SUBLIST ARGV 1 -1 DEPENDENT_LIBRARIES)

	target_link_libraries(${TARGET} PRIVATE ${DEPENDENT_LIBRARIES})

	if (OpenCASCADE_FOUND)
		list(APPEND DEPENDENT_LIBRARIES TKernel freetype)
	endif()
	list(TRANSFORM DEPENDENT_LIBRARIES PREPEND "$<TARGET_FILE_DIR:")
    list(TRANSFORM DEPENDENT_LIBRARIES APPEND ">")	
	install(
			TARGETS ${TARGET}
			RUNTIME_DEPENDENCIES
			DIRECTORIES $<TARGET_FILE_DIR:Qt${VTK_QT_VERSION}::Core> ${DEPENDENT_LIBRARIES}
			PRE_EXCLUDE_REGEXES "api-ms-" "ext-ms-" "spdlog" "freetype" # don't install Windows-provided libs
			POST_EXCLUDE_REGEXES ".*system32/.*\\.dll" "Qt${VTK_QT_VERSION}Core"  # don't install Windows-provided libs
			FRAMEWORK DESTINATION Frameworks
			BUNDLE DESTINATION .
			LIBRARY DESTINATION plugins
			COMPONENT AllPlugins
	)
endfunction()

add_subdirectory(io)
add_subdirectory(algo)