set(TARGET "com.appinf.osp.auth.data")
set(BUNDLE_SPEC "${CMAKE_CURRENT_SOURCE_DIR}/DataBundle.bndlspec")

set(LIBNAME "OSPAuthData")
set(POCO_LIBNAME "Poco${LIBNAME}")
set(LIBVERSION ${SHARED_LIBRARY_VERSION})

# Sources
POCO_SOURCES_AUTO(LIB_SRCS
	src/AuthAdminServiceImpl.cpp
	src/StatementPlaceholderProvider.cpp
)

# Headers
file(GLOB_RECURSE HDRS_G "include/*.h")
POCO_HEADERS_AUTO(LIB_SRCS ${HDRS_G})

# Version Resource
if(MSVC AND BUILD_SHARED_LIBS)
	source_group("Resources" FILES ${PROJECT_SOURCE_DIR}/DLLVersion.rc)
	list(APPEND LIB_SRCS ${PROJECT_SOURCE_DIR}/DLLVersion.rc)
endif()

POCO_SOURCES_AUTO(BUNDLE_SRCS
	src/AuthBundleActivator.cpp
)

# Shared Library
add_library(${LIBNAME} ${LIB_SRCS})
add_library(Poco::OSPAuthData ALIAS ${LIBNAME})
set_target_properties(${LIBNAME}
	PROPERTIES
	VERSION ${LIBVERSION}
	OUTPUT_NAME ${POCO_LIBNAME}
	DEFINE_SYMBOL OSPAuthData_EXPORTS
)

# LDAP
if(ENABLE_OSP_AUTH_LDAP)
	target_compile_definitions(${LIBNAME} PUBLIC POCO_OSP_AUTH_ENABLE_LDAP)
	message(STATUS "Building ${LIBNAME} with LDAP support")
endif()

target_link_libraries(${LIBNAME}
	PUBLIC
	Poco::OSPAuth Poco::OSP Poco::Data Poco::Util Poco::Foundation
)
target_include_directories(${LIBNAME}
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:include>
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Dynamic Library
poco_osp_bundle_module(${TARGET} ${BUNDLE_SRCS})

target_link_libraries(${TARGET}
	PRIVATE
	Poco::OSPAuthData Poco::OSPAuth Poco::OSP Poco::Data Poco::Util Poco::Foundation
)

# Copy Bundle Resources to build directory. The testsuite reads bundle.properties
# from the copy, so it is made again whenever a file of bundle/ changes.
_poco_osp_copy_directory(${TARGET}-resources ${CMAKE_CURRENT_SOURCE_DIR}/bundle ${CMAKE_CURRENT_BINARY_DIR}/bundle)

# Build Bundle
poco_osp_bundle(${TARGET}-bundle
	DEPENDS ${LIBNAME} ${TARGET}
	SPEC ${BUNDLE_SPEC}
	CODE $<TARGET_FILE:${LIBNAME}> $<TARGET_FILE:${TARGET}>
	DESTINATION ${POCO_OSP_BUNDLE_DIR}
	INSTALL_COMPONENT Bundles
)

POCO_INSTALL(${LIBNAME})
POCO_GENERATE_PACKAGE(${LIBNAME})

if(POCO_OSP_ENABLE_TESTS)
	add_subdirectory(testsuite)
endif()
