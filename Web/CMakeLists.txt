set(TARGET "osp.web")
set(BUNDLE_SPEC "${CMAKE_CURRENT_SOURCE_DIR}/WebBundle.bndlspec")

set(LIBNAME "OSPWeb")
set(POCO_LIBNAME "Poco${LIBNAME}")
set(LIBVERSION 3)

set(BUNDLE_DIR ${CMAKE_BINARY_DIR}/${POCO_INSTALL_BUNDLEDIR})

# Sources
set(LIB_SRCS
	src/MediaTypeMapper.cpp
	src/TokenValidator.cpp
	src/WebFilter.cpp
	src/WebFilterExtensionPoint.cpp
	src/WebFilterFactory.cpp
	src/WebRequestHandlerFactory.cpp
	src/WebServerDispatcher.cpp
	src/WebServerExtensionPoint.cpp
	src/WebServerRequestHandler.cpp
	src/WebServerRequestHandlerFactory.cpp
	src/WebServerService.cpp
	src/WebSession.cpp
	src/WebSessionManager.cpp
	src/WebSessionStore.cpp
)

# Version Resource
if(MSVC AND BUILD_SHARED_LIBS)
	source_group("Resources" FILES ${PROJECT_SOURCE_DIR}/ProDLLVersion.rc)
	list(APPEND LIB_SRCS ${PROJECT_SOURCE_DIR}/ProDLLVersion.rc)
endif()

set(BUNDLE_SRCS
	src/WebBundleActivator.cpp
)

# Shared Library
add_library(${LIBNAME} ${LIB_SRCS})
add_library(Poco::OSPWeb ALIAS ${LIBNAME})
set_target_properties(${LIBNAME}
	PROPERTIES
	VERSION ${LIBVERSION} SOVERSION ${LIBVERSION}
	OUTPUT_NAME ${POCO_LIBNAME}
	DEFINE_SYMBOL OSPWeb_EXPORTS
)

target_link_libraries(${LIBNAME} PUBLIC Poco::OSP Poco::Zip Poco::Net Poco::Util Poco::XML Poco::JSON Poco::Foundation)
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

target_link_libraries(${TARGET} OSPWeb Poco::OSP Poco::Zip Poco::Net Poco::Util Poco::XML Poco::JSON Poco::Foundation)

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
