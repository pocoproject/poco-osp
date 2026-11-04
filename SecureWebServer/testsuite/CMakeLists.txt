# Sources
file(GLOB SRCS_G "src/*.cpp")
POCO_SOURCES_AUTO(TEST_SRCS ${SRCS_G})

# Headers
file(GLOB_RECURSE HDRS_G "src/*.h")
POCO_HEADERS_AUTO(TEST_SRCS ${HDRS_G})

# The activator of the bundle.
list(APPEND TEST_SRCS
	${CMAKE_CURRENT_SOURCE_DIR}/../src/SecureWebServerBundleActivator.cpp
)

add_executable(OSPSecureWebServer-testrunner ${TEST_SRCS})
target_include_directories(OSPSecureWebServer-testrunner PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/../src
)
# The self-signed certificate and private key of the test server.
target_compile_definitions(OSPSecureWebServer-testrunner PRIVATE
	"OSPSECUREWEBSERVER_TEST_DATA=\"${CMAKE_CURRENT_SOURCE_DIR}/data/\""
)
add_test(
	NAME OSPSecureWebServer
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	COMMAND OSPSecureWebServer-testrunner -ignore ${POCO_OSP_CPPIGNORE_FILE} -all
)
set_tests_properties(OSPSecureWebServer PROPERTIES ENVIRONMENT POCO_BASE=${PROJECT_SOURCE_DIR})

target_link_libraries(OSPSecureWebServer-testrunner PRIVATE
	OSPTestSupport Poco::OSPWeb Poco::OSP Poco::NetSSL Poco::Crypto Poco::Net Poco::Zip Poco::Util Poco::XML
	Poco::JSON Poco::Foundation Poco::CppUnit
)
poco_osp_link_applink(OSPSecureWebServer-testrunner)
