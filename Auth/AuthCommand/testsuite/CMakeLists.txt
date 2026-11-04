# Sources
file(GLOB SRCS_G "src/*.cpp")
POCO_SOURCES_AUTO(TEST_SRCS ${SRCS_G})

# Headers
file(GLOB_RECURSE HDRS_G "src/*.h")
POCO_HEADERS_AUTO(TEST_SRCS ${HDRS_G})

# The command classes are built from the sources of the bundle, without its
# class loader manifest. The bundle context comes from TemporaryBundle of the
# Web tests.
list(APPEND TEST_SRCS
	${CMAKE_CURRENT_SOURCE_DIR}/../src/AuthCommand.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/../src/PasswdCommand.cpp
)

poco_add_executable(OSPAuthCommand-testrunner ${TEST_SRCS})
add_test(
	NAME OSPAuthCommand
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	COMMAND OSPAuthCommand-testrunner -ignore ${POCO_OSP_CPPIGNORE_FILE} -all
)

target_link_libraries(OSPAuthCommand-testrunner
	PRIVATE OSPTestSupport Poco::OSPAuth Poco::OSPShell Poco::OSP Poco::Zip Poco::Util Poco::XML Poco::Foundation
		Poco::CppUnit
)
target_include_directories(OSPAuthCommand-testrunner PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
