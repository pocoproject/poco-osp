# Sources
file(GLOB SRCS_G "src/*.cpp")
POCO_SOURCES_AUTO(TEST_SRCS ${SRCS_G})

# Headers
file(GLOB_RECURSE HDRS_G "src/*.h")
POCO_HEADERS_AUTO(TEST_SRCS ${HDRS_G})

poco_add_executable(OSPSimpleAuth-testrunner ${TEST_SRCS})
add_test(
	NAME OSPSimpleAuth
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	COMMAND OSPSimpleAuth-testrunner -ignore ${POCO_OSP_CPPIGNORE_FILE} -all
)
set_tests_properties(OSPSimpleAuth PROPERTIES ENVIRONMENT POCO_BASE=${PROJECT_SOURCE_DIR})

target_link_libraries(OSPSimpleAuth-testrunner
	PRIVATE Poco::OSP Poco::Zip Poco::Util Poco::XML Poco::JSON Poco::Foundation Poco::CppUnit
)
target_include_directories(OSPSimpleAuth-testrunner
	PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
