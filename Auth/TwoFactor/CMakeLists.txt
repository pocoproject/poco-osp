set(TARGET "com.appinf.osp.auth.twofactor")
set(BUNDLE_SPEC "${CMAKE_CURRENT_SOURCE_DIR}/TwoFactorBundle.bndlspec")

set(LIBNAME "OSPAuthTwoFactor")
set(POCO_LIBNAME "Poco${LIBNAME}")
set(LIBVERSION ${SHARED_LIBRARY_VERSION})

# Sources
POCO_SOURCES_AUTO(LIB_SRCS
	src/TwoFactorService.cpp
	src/TwoFactorServiceImpl.cpp
	src/SecretGenerator.cpp
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
	src/TwoFactorBundleActivator.cpp
)

# Shared Library
add_library(${LIBNAME} ${LIB_SRCS})
add_library(Poco::OSPAuthTwoFactor ALIAS ${LIBNAME})
set_target_properties(${LIBNAME}
	PROPERTIES
	VERSION ${LIBVERSION}
	OUTPUT_NAME ${POCO_LIBNAME}
	DEFINE_SYMBOL OSPAuthTwoFactor_EXPORTS
)

target_link_libraries(${LIBNAME}
	PUBLIC
	Poco::OSPAuth Poco::OSP Poco::Foundation Poco::Crypto
)
target_include_directories(${LIBNAME}
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:include>
	PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
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
	Poco::OSPAuthTwoFactor Poco::OSPAuth Poco::OSP Poco::Foundation Poco::Crypto
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
