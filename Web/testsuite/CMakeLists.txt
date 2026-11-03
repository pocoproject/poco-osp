# Sources
file(GLOB SRCS_G "src/*.cpp")
POCO_SOURCES_AUTO(TEST_SRCS ${SRCS_G})

# Headers
file(GLOB_RECURSE HDRS_G "src/*.h")
POCO_HEADERS_AUTO(TEST_SRCS ${HDRS_G})

add_executable(OSPWeb-testrunner ${TEST_SRCS})
add_test(
	NAME OSPWeb
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	COMMAND OSPWeb-testrunner -ignore ${POCO_OSP_CPPIGNORE_FILE} -all
)
set_tests_properties(OSPWeb PROPERTIES ENVIRONMENT POCO_BASE=${PROJECT_SOURCE_DIR})

target_link_libraries(OSPWeb-testrunner PRIVATE Poco::OSPWeb Poco::OSP Poco::Zip Poco::Util Poco::XML Poco::Net Poco::JSON Poco::Foundation Poco::CppUnit)
