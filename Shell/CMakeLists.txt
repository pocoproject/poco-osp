set(TARGET "com.appinf.osp.shell")
set(BUNDLE_SPEC "${CMAKE_CURRENT_SOURCE_DIR}/Shell.bndlspec")

set(LIBNAME "OSPShell")
set(POCO_LIBNAME "Poco${LIBNAME}")
set(LIBVERSION 2)

set(BUNDLE_DIR ${CMAKE_BINARY_DIR}/${POCO_INSTALL_BUNDLEDIR})

# Sources
set(LIB_SRCS
	src/AbstractCommand.cpp
	src/Command.cpp
	src/CommandFactory.cpp
	src/CommandProcessor.cpp
	src/HelpCommand.cpp
	src/HelpCommandFactory.cpp
	src/LoginCommand.cpp
	src/LoginCommandFactory.cpp
	src/OSPCommand.cpp
	src/QuitCommand.cpp
	src/QuitCommandFactory.cpp
	src/ResponseStream.cpp
	src/Session.cpp
	src/ShellConnection.cpp
	src/ShellConnectionFactory.cpp
)

# Optional SSH support via Poco::SSH library
if(ENABLE_OSP_SHELL_SSH)
	message(STATUS "OSP Shell: SSH server support enabled (using Poco::SSH)")
else()
	message(STATUS "OSP Shell: SSH server support disabled")
endif()

# Version Resource
if(MSVC AND BUILD_SHARED_LIBS)
	source_group("Resources" FILES ${PROJECT_SOURCE_DIR}/ProDLLVersion.rc)
	list(APPEND LIB_SRCS ${PROJECT_SOURCE_DIR}/ProDLLVersion.rc)
endif()

set(BUNDLE_SRCS
	src/ShellBundleActivator.cpp
	src/CommandExtensionPoint.cpp
)

if(ENABLE_OSP_SHELL_SSH)
	list(APPEND BUNDLE_SRCS src/SSHShellConnection.cpp)
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

target_link_libraries(${LIBNAME} PUBLIC Poco::OSP Poco::Zip Poco::Net Poco::Util Poco::XML Poco::JSON Poco::Foundation)

if(ENABLE_OSP_SHELL_SSH)
	# Poco::SSH treats libssh as PRIVATE; SSHShellConnection.cpp calls ssh_*
	# directly, so OSP/Shell must depend on libssh on its own.
	find_package(libssh CONFIG REQUIRED)
	target_compile_definitions(${LIBNAME} PRIVATE OSP_SHELL_ENABLE_SSH=1)
	target_link_libraries(${LIBNAME} PUBLIC Poco::SSH)
	target_link_libraries(${LIBNAME} PRIVATE ssh)
endif()
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

target_link_libraries(${TARGET} OSPShell Poco::OSP Poco::Zip Poco::Net Poco::Util Poco::XML Poco::JSON Poco::Foundation)

if(ENABLE_OSP_SHELL_SSH)
	# After poco#5339 Poco::SSH links libssh PRIVATE, so SSHShellConnection.cpp
	# (which calls ssh_channel_is_eof / ssh_channel_is_closed directly) needs
	# its own libssh dependency for both include path and linkage.
	find_package(libssh CONFIG REQUIRED)
	target_compile_definitions(${TARGET} PRIVATE OSP_SHELL_ENABLE_SSH=1)
	target_link_libraries(${TARGET} Poco::SSH ssh)
endif()

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
