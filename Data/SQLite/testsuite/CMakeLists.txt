# Sources
file(GLOB SRCS_G "src/*.cpp")
POCO_SOURCES_AUTO(TEST_SRCS ${SRCS_G})

# Headers
file(GLOB_RECURSE HDRS_G "src/*.h")
POCO_HEADERS_AUTO(TEST_SRCS ${HDRS_G})

poco_add_executable(OSPDataSQLite-testrunner ${TEST_SRCS})
# The test of the bundle activator loads the bundle library.
add_dependencies(OSPDataSQLite-testrunner poco.data.sqlite)
target_compile_definitions(OSPDataSQLite-testrunner PRIVATE "SQLITE_BUNDLE_LIBRARY=\"$<TARGET_FILE:poco.data.sqlite>\"")
add_test(
	NAME OSPDataSQLite
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	COMMAND OSPDataSQLite-testrunner -ignore ${POCO_OSP_CPPIGNORE_FILE} -all
)

target_link_libraries(OSPDataSQLite-testrunner
	PRIVATE OSPTestSupport Poco::OSP Poco::DataSQLite Poco::Data Poco::Zip Poco::Util Poco::XML Poco::Foundation
		Poco::CppUnit
)
