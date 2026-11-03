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

# LDAP
if(WIN32)
	if(ENABLE_OSP_AUTH_LDAP)
		target_sources(${LIBNAME} PRIVATE src/LDAPClient.cpp)
		target_sources(${LIBNAME} PRIVATE include/Poco/OSP/Auth/LDAPClient.h)

		target_compile_definitions(${LIBNAME} PUBLIC
			POCO_OSP_AUTH_ENABLE_LDAP
			POCO_OS_FAMILY_WINDOWS
		)
		message(STATUS "Building ${LIBNAME} with WinLDAP support")
		target_link_libraries(${LIBNAME}
			PUBLIC
			Poco::OSP Poco::Foundation
			PRIVATE
			wldap32
		)
	else()
	  target_link_libraries(${LIBNAME}
	  	PUBLIC
	  	Poco::OSP Poco::Foundation
  	)
	endif()
	target_include_directories(${LIBNAME}
		PUBLIC
			$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
			$<INSTALL_INTERFACE:include>
		PRIVATE
			${CMAKE_CURRENT_SOURCE_DIR}/src
	)
else()
	if(ENABLE_OSP_AUTH_LDAP AND APPLE)
		message(FATAL_ERROR "ENABLE_OSP_AUTH_LDAP is not supported on macOS: the LDAP headers are broken there.")
	endif()
	if(ENABLE_OSP_AUTH_LDAP)
		find_package(PkgConfig REQUIRED)
		pkg_check_modules(OPENLDAP REQUIRED IMPORTED_TARGET ldap)
		target_sources(${LIBNAME} PRIVATE src/LDAPClient.cpp)
		target_sources(${LIBNAME} PRIVATE include/Poco/OSP/Auth/LDAPClient.h)

		# NOTE: Newer versions of LDAP library mark some functions as obsolete
		# We enable obsolete functions for now anyway.
		target_compile_definitions(${LIBNAME} PRIVATE LDAP_DEPRECATED)

		target_compile_definitions(${LIBNAME} PUBLIC POCO_OSP_AUTH_ENABLE_LDAP)
		target_link_libraries(${LIBNAME}
			PUBLIC
			Poco::OSP Poco::Foundation
			PRIVATE
			PkgConfig::OPENLDAP
		)
		target_include_directories(${LIBNAME}
			PUBLIC
				$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
				$<INSTALL_INTERFACE:include>
			PRIVATE
				${CMAKE_CURRENT_SOURCE_DIR}/src
		)
		message(STATUS "Building ${LIBNAME} with OpenLDAP support")
	else()
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
		message(STATUS "Building ${LIBNAME} without OpenLDAP support")
	endif()
endif()

set_target_properties(${LIBNAME}
	PROPERTIES
	VERSION ${LIBVERSION} SOVERSION ${LIBVERSION}
	OUTPUT_NAME ${POCO_LIBNAME}
	DEFINE_SYMBOL OSPAuth_EXPORTS
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
	Poco::OSPAuth Poco::OSP Poco::Foundation
)

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
