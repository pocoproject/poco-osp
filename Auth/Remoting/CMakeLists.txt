# Sources
file(GLOB SRCS_G "src/*.cpp")
POCO_SOURCES_AUTO(SRCS ${SRCS_G})

# Headers
file(GLOB_RECURSE HDRS_G "include/*.h")
POCO_HEADERS_AUTO(SRCS ${HDRS_G})

# Version Resource
if(MSVC AND BUILD_SHARED_LIBS)
	source_group("Resources" FILES ${PROJECT_SOURCE_DIR}/DLLVersion.rc)
	list(APPEND SRCS ${PROJECT_SOURCE_DIR}/DLLVersion.rc)
endif()

add_library(OSPAuthRemoting ${SRCS})
add_library(Poco::OSPAuthRemoting ALIAS OSPAuthRemoting)
set_target_properties(OSPAuthRemoting
	PROPERTIES
	VERSION ${SHARED_LIBRARY_VERSION} SOVERSION ${SHARED_LIBRARY_VERSION}
	OUTPUT_NAME PocoOSPAuthRemoting
	DEFINE_SYMBOL OSPAuthRemoting_EXPORTS
)

target_link_libraries(OSPAuthRemoting
	PUBLIC
	Poco::OSP Poco::RemotingNG Poco::Foundation
)
target_include_directories(OSPAuthRemoting
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:include>
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/src
)

POCO_INSTALL(OSPAuthRemoting)
POCO_GENERATE_PACKAGE(OSPAuthRemoting)

if(POCO_OSP_ENABLE_TESTS)
	add_subdirectory(testsuite)
endif()
