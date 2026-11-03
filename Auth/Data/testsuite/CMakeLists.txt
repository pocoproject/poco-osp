# Sources
file(GLOB SRCS_G "src/*.cpp")
POCO_SOURCES_AUTO(TEST_SRCS ${SRCS_G})

# Headers
file(GLOB_RECURSE HDRS_G "src/*.h")
POCO_HEADERS_AUTO(TEST_SRCS ${HDRS_G})

POCO_SOURCES_AUTO_PLAT(TEST_SRCS OFF
	src/WinDriver.cpp
)

add_executable(OSPAuthData-testrunner ${TEST_SRCS})
add_test(
	NAME OSPAuthData
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	COMMAND OSPAuthData-testrunner -ignore ${POCO_OSP_CPPIGNORE_FILE} -all
)
set_tests_properties(OSPAuthData PROPERTIES ENVIRONMENT POCO_BASE=${PROJECT_SOURCE_DIR})

target_link_libraries(OSPAuthData-testrunner PRIVATE Poco::OSPAuthData Poco::OSPAuth Poco::OSP Poco::DataSQLite Poco::Data Poco::Zip Poco::Util Poco::XML Poco::JSON Poco::Foundation Poco::CppUnit)
target_include_directories(OSPAuthData-testrunner
	PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
)
