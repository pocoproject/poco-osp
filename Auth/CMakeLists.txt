set(TARGET "com.appinf.osp.auth")
set(BUNDLE_SPEC "${CMAKE_CURRENT_SOURCE_DIR}/AuthBundle.bndlspec")

set(LIBNAME "OSPAuth")
set(POCO_LIBNAME "Poco${LIBNAME}")
set(LIBVERSION 1)

set(BUNDLE_DIR ${CMAKE_BINARY_DIR}/${POCO_INSTALL_BUNDLEDIR})

# Sources
set(LIB_SRCS
	src/AuthAdminService.cpp
	src/AbstractLDAPAuthAdminService.cpp
	src/PooledAuthAdminService.cpp
	src/AuthNotification.cpp
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
			wldap32.lib
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
	find_package(PkgConfig REQUIRED)
	if(ENABLE_OSP_AUTH_LDAP)
		pkg_check_modules(OPENLDAP REQUIRED ldap)
	else()
		pkg_check_modules(OPENLDAP ldap)
	endif()
	# Note: LDAP headers are severely broken on macOS
	if(OPENLDAP_FOUND AND NOT APPLE)
		target_sources(${LIBNAME} PRIVATE src/LDAPClient.cpp)
		target_sources(${LIBNAME} PRIVATE include/Poco/OSP/Auth/LDAPClient.h)

		# NOTE: Newer versions of LDAP library mark some functions as obsolete
		# We enable obsolete functions for now anyway.
		target_compile_definitions(${LIBNAME} PUBLIC LDAP_DEPRECATED)

		target_compile_definitions(${LIBNAME} PUBLIC POCO_OSP_AUTH_ENABLE_LDAP)
		target_link_libraries(${LIBNAME}
			PUBLIC
			Poco::OSP Poco::Foundation
			${OPENLDAP_LIBRARIES}
		)
		target_include_directories(${LIBNAME}
			PUBLIC
				$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
				$<INSTALL_INTERFACE:include>
			PRIVATE
				${CMAKE_CURRENT_SOURCE_DIR}/src
				${OPENLDAP_INCLUDE_DIR}
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

add_library(Poco::OSPAuth ALIAS ${LIBNAME})
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
	OSPAuth Poco::OSP Poco::Foundation
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
