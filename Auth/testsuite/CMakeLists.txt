# Sources
file(GLOB SRCS_G "src/*.cpp")
POCO_SOURCES_AUTO(TEST_SRCS ${SRCS_G})

# Headers
file(GLOB_RECURSE HDRS_G "src/*.h")
POCO_HEADERS_AUTO(TEST_SRCS ${HDRS_G})

poco_add_executable(OSPAuth-testrunner ${TEST_SRCS})
add_test(
	NAME OSPAuth
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	COMMAND OSPAuth-testrunner -ignore ${POCO_OSP_CPPIGNORE_FILE} -all
)

target_link_libraries(OSPAuth-testrunner
	PRIVATE Poco::OSPAuth Poco::OSP Poco::Net Poco::Zip Poco::Util Poco::XML Poco::JSON Poco::Foundation Poco::CppUnit
)
