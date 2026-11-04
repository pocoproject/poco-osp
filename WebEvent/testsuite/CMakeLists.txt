# Sources
file(GLOB SRCS_G "src/*.cpp")
POCO_SOURCES_AUTO(TEST_SRCS ${SRCS_G})

# Headers
file(GLOB_RECURSE HDRS_G "src/*.h")
POCO_HEADERS_AUTO(TEST_SRCS ${HDRS_G})

poco_add_executable(OSPWebEvent-testrunner ${TEST_SRCS})
# The test of the bundle activator loads the bundle library.
add_dependencies(OSPWebEvent-testrunner com.appinf.osp.webevent)
target_compile_definitions(OSPWebEvent-testrunner PRIVATE
	"WEBEVENT_BUNDLE_LIBRARY=\"$<TARGET_FILE:com.appinf.osp.webevent>\""
)
add_test(
	NAME OSPWebEvent
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	COMMAND OSPWebEvent-testrunner -ignore ${POCO_OSP_CPPIGNORE_FILE} -all
)
set_tests_properties(OSPWebEvent PROPERTIES ENVIRONMENT POCO_BASE=${PROJECT_SOURCE_DIR})

target_link_libraries(OSPWebEvent-testrunner
	PRIVATE
	OSPTestSupport Poco::OSPWebEvent Poco::OSPWeb Poco::OSP Poco::Net Poco::Zip Poco::Util Poco::Foundation
	Poco::CppUnit
)

# The TLS tests use the certificate for localhost of the SecureMail test suite.
if(ENABLE_OSP_NETSSL)
	target_compile_definitions(OSPWebEvent-testrunner PRIVATE OSP_WEBEVENT_ENABLE_TLS=1)
	target_link_libraries(OSPWebEvent-testrunner PRIVATE Poco::NetSSL Poco::Crypto)
	poco_osp_link_applink(OSPWebEvent-testrunner)
endif()
