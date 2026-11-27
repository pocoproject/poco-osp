set(TARGET "com.appinf.osp.auth")
set(BUNDLE_SPEC "${CMAKE_CURRENT_SOURCE_DIR}/AuthBundle.bndlspec")

set(LIBNAME "OSPAuth")
set(POCO_LIBNAME "Poco${LIBNAME}")
set(LIBVERSION ${SHARED_LIBRARY_VERSION})

# Sources
POCO_SOURCES_AUTO(LIB_SRCS
	src/AuthAdminService.cpp
	src/AbstractLDAPAuthAdminService.cpp
	src/PooledAuthAdminService.cpp
	src/AuthNotification.cpp
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
add_library(Poco::OSPAuth ALIAS ${LIBNAME})

target_link_libraries(${LIBNAME}
	PUBLIC
	Poco::OSP Poco::Foundation
)
target_include_directories(${LIBNAME}
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:include>
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/src
)

# LDAP
if(ENABLE_OSP_AUTH_LDAP)
	if(WIN32)
		target_link_libraries(${LIBNAME} PRIVATE wldap32)
		message(STATUS "Building ${LIBNAME} with WinLDAP support")
	elseif(APPLE)
		message(FATAL_ERROR "ENABLE_OSP_AUTH_LDAP is not supported on macOS: the system LDAP library is deprecated "
			"there and provides no pkg-config module.")
	else()
		find_package(PkgConfig REQUIRED)
		pkg_check_modules(OPENLDAP REQUIRED IMPORTED_TARGET ldap)
		target_link_libraries(${LIBNAME} PRIVATE PkgConfig::OPENLDAP)
		message(STATUS "Building ${LIBNAME} with OpenLDAP support")
	endif()
	target_sources(${LIBNAME} PRIVATE src/LDAPClient.cpp)
	target_compile_definitions(${LIBNAME} PUBLIC POCO_OSP_AUTH_ENABLE_LDAP)
else()
	message(STATUS "Building ${LIBNAME} without LDAP support")
endif()

set_target_properties(${LIBNAME}
	PROPERTIES
	VERSION ${LIBVERSION} SOVERSION ${LIBVERSION}
	OUTPUT_NAME ${POCO_LIBNAME}
	DEFINE_SYMBOL OSPAuth_EXPORTS
)

# Dynamic Library
poco_osp_bundle_module(${TARGET} ${BUNDLE_SRCS})

target_link_libraries(${TARGET}
	PRIVATE
	Poco::OSPAuth Poco::OSP Poco::Foundation
)

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
