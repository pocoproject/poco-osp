# Sources
file(GLOB SRCS_G "src/*.cpp")
POCO_SOURCES_AUTO(TEST_SRCS ${SRCS_G})

# Headers
file(GLOB_RECURSE HDRS_G "src/*.h")
POCO_HEADERS_AUTO(TEST_SRCS ${HDRS_G})

poco_add_executable(OSPMail-testrunner ${TEST_SRCS})
add_test(
	NAME OSPMail
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	COMMAND OSPMail-testrunner -ignore ${POCO_OSP_CPPIGNORE_FILE} -all
)

target_link_libraries(OSPMail-testrunner
	PRIVATE OSPTestSupport Poco::OSPMail Poco::OSP Poco::Net Poco::Util Poco::Foundation Poco::CppUnit
)
