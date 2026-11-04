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

add_library(OSPUnit ${SRCS})
add_library(Poco::OSPUnit ALIAS OSPUnit)
set_target_properties(OSPUnit
	PROPERTIES
	VERSION ${SHARED_LIBRARY_VERSION} SOVERSION ${SHARED_LIBRARY_VERSION}
	OUTPUT_NAME PocoOSPUnit
	DEFINE_SYMBOL OSPUnit_EXPORTS
)

target_link_libraries(OSPUnit PUBLIC Poco::OSP Poco::CppUnit Poco::Foundation)
target_include_directories(OSPUnit
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:include>
	PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Not installed: the public headers include CppUnit headers, which POCO installs only with
# ENABLE_INSTALL_CPPUNIT, and POCO exports no Poco::CppUnit. The test runner bundle ships the library.

if(POCO_OSP_ENABLE_TESTS)
	add_subdirectory(testsuite)
endif()

if(POCO_OSP_ENABLE_SAMPLES)
	add_subdirectory(samples)
endif()
