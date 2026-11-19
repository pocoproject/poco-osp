# Sources
file(GLOB SRCS_G "src/*.cpp")
POCO_SOURCES_AUTO(TEST_SRCS ${SRCS_G})

# Headers
file(GLOB_RECURSE HDRS_G "src/*.h")
POCO_HEADERS_AUTO(TEST_SRCS ${HDRS_G})

add_executable(OSPAuthRemoting-testrunner ${TEST_SRCS})
add_test(
	NAME OSPAuthRemoting
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	COMMAND OSPAuthRemoting-testrunner -ignore ${POCO_OSP_CPPIGNORE_FILE} -all
)
set_tests_properties(OSPAuthRemoting PROPERTIES ENVIRONMENT POCO_BASE=${PROJECT_SOURCE_DIR})

target_link_libraries(OSPAuthRemoting-testrunner PRIVATE
	OSPTestSupport OSPTestSupportNet Poco::OSPAuthRemoting Poco::OSP Poco::RemotingNG Poco::Net Poco::Zip Poco::Util
	Poco::XML Poco::Foundation Poco::CppUnit
)
