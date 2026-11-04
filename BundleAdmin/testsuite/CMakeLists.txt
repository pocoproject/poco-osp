# Sources
file(GLOB SRCS_G "src/*.cpp")
POCO_SOURCES_AUTO(TEST_SRCS ${SRCS_G})

# Headers
file(GLOB_RECURSE HDRS_G "src/*.h")
POCO_HEADERS_AUTO(TEST_SRCS ${HDRS_G})

poco_add_executable(OSPBundleAdmin-testrunner ${TEST_SRCS})
# The tests load the request handler factories from the bundle library, with
# the bundle properties of the bundle.
add_dependencies(OSPBundleAdmin-testrunner com.appinf.osp.bundleadmin)
target_compile_definitions(OSPBundleAdmin-testrunner PRIVATE
	"BUNDLEADMIN_BUNDLE_LIBRARY=\"$<TARGET_FILE:com.appinf.osp.bundleadmin>\""
	"BUNDLEADMIN_BUNDLE_PROPERTIES=\"${CMAKE_CURRENT_SOURCE_DIR}/../bundle/bundle.properties\""
)
add_test(
	NAME OSPBundleAdmin
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	COMMAND OSPBundleAdmin-testrunner -ignore ${POCO_OSP_CPPIGNORE_FILE} -all
)

target_link_libraries(OSPBundleAdmin-testrunner
	PRIVATE OSPTestSupport Poco::OSPWeb Poco::OSP Poco::Net Poco::Zip Poco::Util Poco::XML Poco::Foundation
		Poco::CppUnit
)
