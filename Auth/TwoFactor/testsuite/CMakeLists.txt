# Sources
file(GLOB SRCS_G "src/*.cpp")
POCO_SOURCES_AUTO(TEST_SRCS ${SRCS_G})

# Headers
file(GLOB_RECURSE HDRS_G "src/*.h")
POCO_HEADERS_AUTO(TEST_SRCS ${HDRS_G})

poco_add_executable(OSPAuthTwoFactor-testrunner ${TEST_SRCS})
# The tests of the bundle activator load the bundle library.
add_dependencies(OSPAuthTwoFactor-testrunner com.appinf.osp.auth.twofactor)
target_compile_definitions(OSPAuthTwoFactor-testrunner
	PRIVATE "TWOFACTOR_BUNDLE_LIBRARY=\"$<TARGET_FILE:com.appinf.osp.auth.twofactor>\""
)
add_test(
	NAME OSPAuthTwoFactor
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	COMMAND OSPAuthTwoFactor-testrunner -ignore ${POCO_OSP_CPPIGNORE_FILE} -all
)
set_tests_properties(OSPAuthTwoFactor PROPERTIES ENVIRONMENT POCO_BASE=${PROJECT_SOURCE_DIR})

target_link_libraries(OSPAuthTwoFactor-testrunner
	PRIVATE
	OSPTestSupport Poco::OSPAuthTwoFactor Poco::OSPAuth Poco::OSP Poco::Crypto Poco::Zip Poco::Util Poco::XML Poco::JSON
	Poco::Foundation Poco::CppUnit
)
poco_osp_link_applink(OSPAuthTwoFactor-testrunner)
