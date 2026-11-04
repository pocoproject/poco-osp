set(TARGET "com.appinf.osp.shell")
set(BUNDLE_SPEC "${CMAKE_CURRENT_SOURCE_DIR}/Shell.bndlspec")

set(LIBNAME "OSPShell")
set(POCO_LIBNAME "Poco${LIBNAME}")
set(LIBVERSION ${SHARED_LIBRARY_VERSION})

# Sources
POCO_SOURCES_AUTO(LIB_SRCS
	src/AbstractCommand.cpp
	src/Command.cpp
	src/CommandFactory.cpp
	src/CommandProcessor.cpp
	src/HelpCommand.cpp
	src/HelpCommandFactory.cpp
	src/LoginCommand.cpp
	src/LoginCommandFactory.cpp
	src/LoginFailures.cpp
	src/OSPCommand.cpp
	src/QuitCommand.cpp
	src/QuitCommandFactory.cpp
	src/ResponseStream.cpp
	src/Session.cpp
	src/SessionTracker.cpp
	src/ShellConnection.cpp
	src/ShellConnectionFactory.cpp
	src/ShellServer.cpp
)

# Headers
file(GLOB_RECURSE HDRS_G "include/*.h")
POCO_HEADERS_AUTO(LIB_SRCS ${HDRS_G})

# Optional SSH support via Poco::SSH library
if(ENABLE_OSP_SHELL_SSH)
	message(STATUS "OSP Shell: SSH server support enabled (using Poco::SSH)")
else()
	message(STATUS "OSP Shell: SSH server support disabled")
endif()

# Version Resource
if(MSVC AND BUILD_SHARED_LIBS)
	source_group("Resources" FILES ${PROJECT_SOURCE_DIR}/DLLVersion.rc)
	list(APPEND LIB_SRCS ${PROJECT_SOURCE_DIR}/DLLVersion.rc)
endif()

POCO_SOURCES_AUTO(BUNDLE_SRCS
	src/ShellBundleActivator.cpp
	src/CommandExtensionPoint.cpp
)

if(ENABLE_OSP_SHELL_SSH)
	POCO_SOURCES_AUTO(BUNDLE_SRCS src/SSHShellConnection.cpp)
endif()

# Shared Library
add_library(${LIBNAME} ${LIB_SRCS})
add_library(Poco::OSPShell ALIAS ${LIBNAME})
set_target_properties(${LIBNAME}
	PROPERTIES
	VERSION ${LIBVERSION} SOVERSION ${LIBVERSION}
	OUTPUT_NAME ${POCO_LIBNAME}
	DEFINE_SYMBOL OSPShell_EXPORTS
)

target_link_libraries(${LIBNAME} PUBLIC Poco::OSP Poco::Net Poco::Util Poco::Foundation PRIVATE Poco::Zip Poco::XML Poco::JSON)

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

target_link_libraries(${TARGET} PRIVATE Poco::OSPShell Poco::OSP Poco::Zip Poco::Net Poco::Util Poco::XML Poco::JSON Poco::Foundation)

if(ENABLE_OSP_SHELL_SSH)
	# Poco::SSH links libssh PRIVATE, so SSHShellConnection.cpp
	# (which calls ssh_channel_is_eof / ssh_channel_is_closed directly) needs
	# its own libssh dependency for both include path and linkage.
	find_package(libssh CONFIG REQUIRED)
	target_compile_definitions(${TARGET} PRIVATE OSP_SHELL_ENABLE_SSH=1)
	target_link_libraries(${TARGET} PRIVATE Poco::SSH ssh)
endif()

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
