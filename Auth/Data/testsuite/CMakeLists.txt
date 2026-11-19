# Sources
file(GLOB SRCS_G "src/*.cpp")
POCO_SOURCES_AUTO(TEST_SRCS ${SRCS_G})

# Headers
file(GLOB_RECURSE HDRS_G "src/*.h")
POCO_HEADERS_AUTO(TEST_SRCS ${HDRS_G})

poco_add_executable(OSPAuthData-testrunner ${TEST_SRCS})
# The tests of the bundle activator load the bundle library.
add_dependencies(OSPAuthData-testrunner com.appinf.osp.auth.data)
target_compile_definitions(OSPAuthData-testrunner
	PRIVATE "DATA_BUNDLE_LIBRARY=\"$<TARGET_FILE:com.appinf.osp.auth.data>\""
)
# The tests read bundle/bundle.properties from the copy in the build directory.
add_dependencies(OSPAuthData-testrunner com.appinf.osp.auth.data-resources)
add_test(
	NAME OSPAuthData
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	COMMAND OSPAuthData-testrunner -ignore ${POCO_OSP_CPPIGNORE_FILE} -all
)
set_tests_properties(OSPAuthData PROPERTIES ENVIRONMENT POCO_BASE=${PROJECT_SOURCE_DIR})

target_link_libraries(OSPAuthData-testrunner
	PRIVATE
	OSPTestSupport Poco::OSPAuthData Poco::OSPAuth Poco::OSP Poco::DataSQLite Poco::Data Poco::Zip Poco::Util Poco::XML
	Poco::JSON Poco::Foundation Poco::CppUnit
)
