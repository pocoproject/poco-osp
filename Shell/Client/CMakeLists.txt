# Sources
file(GLOB SRCS_G "src/*.cpp")
POCO_SOURCES_AUTO(SRCS ${SRCS_G})

# Version Resource
if(MSVC)
	source_group("Resources" FILES ${PROJECT_SOURCE_DIR}/ProDLLVersion.rc)
	list(APPEND SRCS ${PROJECT_SOURCE_DIR}/ProDLLVersion.rc)
endif()

find_package(Libedit)

add_executable(OSPShellClient ${SRCS})
set_target_properties(OSPShellClient
	PROPERTIES
	OUTPUT_NAME ospsh
)

if(LIBEDIT_FOUND)
	target_compile_definitions(OSPShellClient PUBLIC OSPSH_HAVE_EDITLINE)
	target_link_libraries(OSPShellClient PUBLIC Poco::OSPShell Poco::OSP Poco::Net Poco::Util ${LIBEDIT_LIBRARIES})
	target_include_directories(OSPShellClient
		PRIVATE ${LIBEDIT_INCLUDE_DIR}
	)
	message(STATUS "Building ospsh with libedit")
else()
	target_link_libraries(OSPShellClient PUBLIC Poco::OSPShell Poco::OSP Poco::Net Poco::Util)
endif()

install(
	TARGETS OSPShellClient EXPORT OSPShellClientTargets
	LIBRARY DESTINATION lib${LIB_SUFFIX}
	ARCHIVE DESTINATION lib${LIB_SUFFIX}
	RUNTIME DESTINATION bin
	INCLUDES DESTINATION include
)
