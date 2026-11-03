# Sources
file(GLOB SRCS_G "src/*.cpp")
POCO_SOURCES_AUTO(SRCS ${SRCS_G})

# Version Resource
if(MSVC)
	source_group("Resources" FILES ${PROJECT_SOURCE_DIR}/DLLVersion.rc)
	list(APPEND SRCS ${PROJECT_SOURCE_DIR}/DLLVersion.rc)
endif()

add_executable(OSPShellClient ${SRCS})
set_target_properties(OSPShellClient
	PROPERTIES
	OUTPUT_NAME ospsh
)

target_link_libraries(OSPShellClient PRIVATE Poco::OSPShell Poco::OSP Poco::Net Poco::Util)

# Line editing and history are optional: without libedit, ospsh reads plain lines.
# On macOS, libedit is a system library of the SDK.
if(APPLE)
	set(_poco_osp_libedit edit)
else()
	find_package(PkgConfig)
	if(PKG_CONFIG_FOUND)
		pkg_check_modules(LIBEDIT IMPORTED_TARGET libedit)
	endif()
	if(LIBEDIT_FOUND)
		set(_poco_osp_libedit PkgConfig::LIBEDIT)
	endif()
endif()
if(DEFINED _poco_osp_libedit)
	target_compile_definitions(OSPShellClient PRIVATE OSPSH_HAVE_EDITLINE)
	target_link_libraries(OSPShellClient PRIVATE ${_poco_osp_libedit})
	message(STATUS "Building ospsh with libedit")
else()
	message(STATUS "Building ospsh without libedit")
endif()
unset(_poco_osp_libedit)

install(
	TARGETS OSPShellClient EXPORT OSPShellClientTargets
	LIBRARY DESTINATION lib${LIB_SUFFIX}
	ARCHIVE DESTINATION lib${LIB_SUFFIX}
	RUNTIME DESTINATION bin
	INCLUDES DESTINATION include
)
