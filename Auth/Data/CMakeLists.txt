set(TARGET "com.appinf.osp.auth.data")
set(BUNDLE_SPEC "${CMAKE_CURRENT_SOURCE_DIR}/DataBundle.bndlspec")

set(LIBNAME "OSPAuthData")
set(POCO_LIBNAME "Poco${LIBNAME}")
set(LIBVERSION 1)

set(BUNDLE_DIR ${CMAKE_BINARY_DIR}/${POCO_INSTALL_BUNDLEDIR})

# Sources
set(LIB_SRCS
	src/AuthAdminServiceImpl.cpp
	src/StatementPlaceholderProvider.cpp
)

# Version Resource
if(MSVC AND BUILD_SHARED_LIBS)
	source_group("Resources" FILES ${PROJECT_SOURCE_DIR}/ProDLLVersion.rc)
	list(APPEND LIB_SRCS ${PROJECT_SOURCE_DIR}/ProDLLVersion.rc)
endif()

set(BUNDLE_SRCS
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
if(WIN32)
	if(ENABLE_OSP_AUTH_LDAP)
		target_compile_definitions(${LIBNAME} PUBLIC POCO_OSP_AUTH_ENABLE_LDAP)
		message(STATUS "Building ${LIBNAME} with LDAP support")
	endif()
else()
	# Note: LDAP headers are severely broken on macOS
	if(OPENLDAP_FOUND AND NOT APPLE)
		target_compile_definitions(${LIBNAME} PUBLIC POCO_OSP_AUTH_ENABLE_LDAP)
		message(STATUS "Building ${LIBNAME} with OpenLDAP support")
	endif()
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
	Poco::OSPAuthData Poco::OSPAuth Poco::OSP Poco::Data Poco::Util Poco::Foundation
)

# Copy Bundle Resources to build directory
add_custom_command(TARGET ${TARGET}
	POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/bundle ${CMAKE_CURRENT_BINARY_DIR}/bundle
)

# Build Bundle
poco_osp_bundle(${TARGET}-bundle
	DEPENDS ${LIBNAME} ${TARGET}
	SPEC ${BUNDLE_SPEC}
	CODE $<TARGET_FILE:${LIBNAME}> $<TARGET_FILE:${TARGET}>
	DESTINATION ${BUNDLE_DIR}
)

install(
	DIRECTORY	 ${BUNDLE_DIR}/
	DESTINATION  ${POCO_INSTALL_BUNDLEDIR}
	COMPONENT    Bundles
	FILES_MATCHING PATTERN "${TARGET}*.bndl"
)

POCO_INSTALL(${LIBNAME})
POCO_GENERATE_PACKAGE(${LIBNAME})

if(ENABLE_TESTS)
	add_subdirectory(testsuite)
endif()
