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
add_library(${TARGET} MODULE ${BUNDLE_SRCS})
set_target_properties(${TARGET}
	PROPERTIES
	PREFIX ""
)

# On macOS, use .dylib instead of .so
if(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
	set_target_properties(${TARGET} PROPERTIES SUFFIX ".dylib")
endif()

target_link_libraries(${TARGET}
	PRIVATE
	Poco::OSPAuthData Poco::OSPAuth Poco::OSP Poco::Data Poco::Util Poco::Foundation
)

# Copy Bundle Resources to build directory. The testsuite reads bundle.properties
# from the copy, so it is made again whenever a file of bundle/ changes.
file(GLOB_RECURSE BUNDLE_RESOURCE_FILES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/bundle/*)
add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/BundleResources.copied
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/bundle ${CMAKE_CURRENT_BINARY_DIR}/bundle
	COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/BundleResources.copied
	DEPENDS ${BUNDLE_RESOURCE_FILES}
	VERBATIM
)
add_custom_target(${TARGET}-resources ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/BundleResources.copied)

# Build Bundle
poco_osp_bundle(${TARGET}-bundle
	DEPENDS ${LIBNAME} ${TARGET}
	SPEC ${BUNDLE_SPEC}
	CODE $<TARGET_FILE:${LIBNAME}> $<TARGET_FILE:${TARGET}>
	DESTINATION ${POCO_OSP_BUNDLE_DIR}
)

install(
	DIRECTORY	 ${POCO_OSP_BUNDLE_DIR}/
	DESTINATION  ${POCO_INSTALL_BUNDLEDIR}
	COMPONENT    Bundles
	FILES_MATCHING PATTERN "${TARGET}*.bndl"
)

POCO_INSTALL(${LIBNAME})
POCO_GENERATE_PACKAGE(${LIBNAME})

if(POCO_OSP_ENABLE_TESTS)
	add_subdirectory(testsuite)
endif()
