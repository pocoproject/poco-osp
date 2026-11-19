# Sources
file(GLOB SRCS_G "src/*.cpp")
POCO_SOURCES_AUTO(TEST_SRCS ${SRCS_G})

# Headers
file(GLOB_RECURSE HDRS_G "src/*.h")
POCO_HEADERS_AUTO(TEST_SRCS ${HDRS_G})

poco_add_executable(OSPNet-testrunner ${TEST_SRCS})
# The test of the bundle activator loads the bundle library.
add_dependencies(OSPNet-testrunner poco.net)
target_compile_definitions(OSPNet-testrunner PRIVATE "NET_BUNDLE_LIBRARY=\"$<TARGET_FILE:poco.net>\"")
add_test(
	NAME OSPNet
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	COMMAND OSPNet-testrunner -ignore ${POCO_OSP_CPPIGNORE_FILE} -all
)

target_link_libraries(OSPNet-testrunner
	PRIVATE OSPTestSupport Poco::OSP Poco::Net Poco::Zip Poco::Util Poco::XML Poco::Foundation Poco::CppUnit
)
