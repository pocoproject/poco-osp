# Sources
file(GLOB SRCS_G "src/*.cpp")
if(NOT ENABLE_OSP_SHELL_CLIENT)
	list(REMOVE_ITEM SRCS_G "${CMAKE_CURRENT_SOURCE_DIR}/src/ClientTest.cpp")
endif()
if(NOT ENABLE_OSP_SHELL_SSH)
	list(REMOVE_ITEM SRCS_G "${CMAKE_CURRENT_SOURCE_DIR}/src/SSHShellConnectionTest.cpp")
endif()
if(NOT POCO_OSP_ENABLE_SAMPLES)
	list(REMOVE_ITEM SRCS_G "${CMAKE_CURRENT_SOURCE_DIR}/src/EchoCommandTest.cpp")
endif()
POCO_SOURCES_AUTO(TEST_SRCS ${SRCS_G})

# Headers
file(GLOB_RECURSE HDRS_G "src/*.h")
POCO_HEADERS_AUTO(TEST_SRCS ${HDRS_G})

# The command extension point is part of the shell bundle, not of the library.
list(APPEND TEST_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/../src/CommandExtensionPoint.cpp)

poco_add_executable(OSPShell-testrunner ${TEST_SRCS})
add_test(
	NAME OSPShell
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	COMMAND OSPShell-testrunner -ignore ${POCO_OSP_CPPIGNORE_FILE} -all
)

target_link_libraries(OSPShell-testrunner
	PRIVATE OSPTestSupport Poco::OSPShell Poco::OSP Poco::Net Poco::Util Poco::XML Poco::Foundation Poco::CppUnit
)
target_include_directories(OSPShell-testrunner PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)

# The tests load the shell and command bundles that the build creates, with
# the bundle properties and extensions of their source directories.
add_dependencies(OSPShell-testrunner
	com.appinf.osp.shell
	com.appinf.osp.shell.commands
	com.appinf.osp.shell.loglevelcommand
)
target_compile_definitions(OSPShell-testrunner
	PRIVATE
		"OSPSHELL_TEST_SHELL_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/..\""
		"OSPSHELL_TEST_SHELL_LIBRARY=\"$<TARGET_FILE:com.appinf.osp.shell>\""
		"OSPSHELL_TEST_COMMANDS_LIBRARY=\"$<TARGET_FILE:com.appinf.osp.shell.commands>\""
		"OSPSHELL_TEST_LOGLEVEL_LIBRARY=\"$<TARGET_FILE:com.appinf.osp.shell.loglevelcommand>\""
)

# Two libraries that export a command factory class with the same name, whose
# commands write the name of their library.
foreach(name first second)
	add_library(OSPShellTestLibrary_${name} MODULE library/TestCommandLibrary.cpp)
	# Hidden symbols, so that the two libraries do not use each other's code.
	set_target_properties(OSPShellTestLibrary_${name}
		PROPERTIES CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON
	)
	target_compile_definitions(OSPShellTestLibrary_${name} PRIVATE "OSPSHELL_TEST_LIBRARY_NAME=\"${name}\"")
	target_link_libraries(OSPShellTestLibrary_${name} PRIVATE Poco::OSPShell Poco::Foundation)
	add_dependencies(OSPShell-testrunner OSPShellTestLibrary_${name})
endforeach()
target_compile_definitions(OSPShell-testrunner
	PRIVATE
		"OSPSHELL_TEST_FIRST_LIBRARY=\"$<TARGET_FILE:OSPShellTestLibrary_first>\""
		"OSPSHELL_TEST_SECOND_LIBRARY=\"$<TARGET_FILE:OSPShellTestLibrary_second>\""
)

# The client tests run ospsh against a shell server of the test runner.
if(ENABLE_OSP_SHELL_CLIENT)
	add_dependencies(OSPShell-testrunner OSPShellClient)
	target_compile_definitions(OSPShell-testrunner PRIVATE "OSPSHELL_TEST_CLIENT=\"$<TARGET_FILE:OSPShellClient>\"")
endif()

# The tests of the echo command sample load the sample's library.
if(POCO_OSP_ENABLE_SAMPLES)
	add_dependencies(OSPShell-testrunner com.appinf.osp.samples.echocommand)
	target_compile_definitions(OSPShell-testrunner
		PRIVATE "OSPSHELL_TEST_ECHO_LIBRARY=\"$<TARGET_FILE:com.appinf.osp.samples.echocommand>\""
	)
endif()

# The SSH tests run the SSH session of the shell bundle with an SSH server and client of Poco::SSH.
if(ENABLE_OSP_SHELL_SSH)
	target_sources(OSPShell-testrunner PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src/SSHShellConnection.cpp)
	target_compile_definitions(OSPShell-testrunner PRIVATE OSP_SHELL_ENABLE_SSH=1)
	target_link_libraries(OSPShell-testrunner PRIVATE Poco::SSH ssh)
endif()
