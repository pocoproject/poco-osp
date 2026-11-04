# Sources
file(GLOB SRCS_G "src/*.cpp")
POCO_SOURCES_AUTO(TEST_SRCS ${SRCS_G})

# Headers
file(GLOB_RECURSE HDRS_G "src/*.h")
POCO_HEADERS_AUTO(TEST_SRCS ${HDRS_G})

# The bundles for the activator are the ones of the Web test suite.
list(APPEND TEST_SRCS ${PROJECT_SOURCE_DIR}/Web/testsuite/src/TemporaryBundle.cpp)

add_executable(OSPNet-testrunner ${TEST_SRCS})
target_include_directories(OSPNet-testrunner PRIVATE ${PROJECT_SOURCE_DIR}/Web/testsuite/src)
# The test of the bundle activator loads the bundle library.
add_dependencies(OSPNet-testrunner poco.net)
target_compile_definitions(OSPNet-testrunner PRIVATE "NET_BUNDLE_LIBRARY=\"$<TARGET_FILE:poco.net>\"")
add_test(
	NAME OSPNet
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	COMMAND OSPNet-testrunner -ignore ${POCO_OSP_CPPIGNORE_FILE} -all
)

target_link_libraries(OSPNet-testrunner PRIVATE Poco::OSP Poco::Net Poco::Zip Poco::Util Poco::XML Poco::Foundation Poco::CppUnit)
