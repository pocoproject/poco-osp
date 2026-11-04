# Sources
file(GLOB SRCS_G "src/*.cpp")
POCO_SOURCES_AUTO(TEST_SRCS ${SRCS_G})

# Headers
file(GLOB_RECURSE HDRS_G "src/*.h")
POCO_HEADERS_AUTO(TEST_SRCS ${HDRS_G})

poco_add_executable(OSPRemotingNGTCP-testrunner ${TEST_SRCS})
# The tests of the bundle activator load the bundle library.
add_dependencies(OSPRemotingNGTCP-testrunner com.appinf.osp.remotingng.tcp)
target_compile_definitions(OSPRemotingNGTCP-testrunner PRIVATE "TCP_BUNDLE_LIBRARY=\"$<TARGET_FILE:com.appinf.osp.remotingng.tcp>\"")
add_test(
	NAME OSPRemotingNGTCP
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	COMMAND OSPRemotingNGTCP-testrunner -ignore ${POCO_OSP_CPPIGNORE_FILE} -all
)

target_link_libraries(OSPRemotingNGTCP-testrunner
	PRIVATE OSPTestSupport Poco::OSP Poco::RemotingNGTCP Poco::RemotingNG Poco::Net Poco::Zip Poco::Util Poco::XML
		Poco::Foundation Poco::CppUnit
)
