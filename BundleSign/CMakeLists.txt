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

add_library(OSPBundleSign ${SRCS})
add_library(Poco::OSPBundleSign ALIAS OSPBundleSign)
set_target_properties(OSPBundleSign
	PROPERTIES
	VERSION ${SHARED_LIBRARY_VERSION} SOVERSION ${SHARED_LIBRARY_VERSION}
	OUTPUT_NAME PocoOSPBundleSign
	DEFINE_SYMBOL OSPBundleSign_EXPORTS
)

target_link_libraries(OSPBundleSign PUBLIC Poco::OSP Poco::Crypto Poco::Foundation PRIVATE Poco::XML Poco::Util Poco::Zip)
target_include_directories(OSPBundleSign
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:include>
	PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

POCO_INSTALL(OSPBundleSign)
POCO_GENERATE_PACKAGE(OSPBundleSign)

if(POCO_OSP_ENABLE_TESTS)
	add_subdirectory(testsuite)
endif()
