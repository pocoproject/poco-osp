# Sources
file(GLOB SRCS_G "src/*.cpp")
list(REMOVE_ITEM SRCS_G "${CMAKE_CURRENT_SOURCE_DIR}/src/TestBundle.cpp")
POCO_SOURCES_AUTO(TEST_SRCS ${SRCS_G})

# Headers
file(GLOB_RECURSE HDRS_G "src/*.h")
POCO_HEADERS_AUTO(TEST_SRCS ${HDRS_G})

poco_add_executable(OSP-testrunner ${TEST_SRCS})
add_test(
	NAME OSP
	WORKING_DIRECTORY $<TARGET_FILE_DIR:OSP-testrunner>
	COMMAND OSP-testrunner -ignore ${POCO_OSP_CPPIGNORE_FILE} -all
)
# A test that blocks fails the run instead of waiting for the default timeout of ctest.
set_tests_properties(OSP PROPERTIES
	ENVIRONMENT "POCO_BASE=${PROJECT_SOURCE_DIR};POCO_OSP_TEST_BUNDLES=${CMAKE_CURRENT_BINARY_DIR}/bundles"
	TIMEOUT 600
)
add_test(
	NAME OSP-without-test-bundles
	COMMAND OSP-testrunner -all
)
set_tests_properties(OSP-without-test-bundles PROPERTIES
	ENVIRONMENT_MODIFICATION POCO_OSP_TEST_BUNDLES=unset:
	PASS_REGULAR_EXPRESSION "POCO_OSP_TEST_BUNDLES must name the directory of the test bundles"
)

target_link_libraries(OSP-testrunner PRIVATE Poco::OSP Poco::Zip Poco::Util Poco::XML Poco::Foundation Poco::CppUnit)

poco_osp_bundle_module(TestBundle src/TestBundle.cpp)

target_link_libraries(TestBundle PRIVATE Poco::OSP Poco::Foundation)

# The tests read the bundles from a copy in the build directory, which also holds
# the TestBundle library of bundle9.
set(TEST_BUNDLES_DIR ${CMAKE_CURRENT_BINARY_DIR}/bundles)
set(TEST_BUNDLE9_CODE_DIR ${TEST_BUNDLES_DIR}/com.appinf.osp.bundle9_1.0.0/bin/${OSP_OSNAME}/${OSP_OSARCH})
_poco_osp_copy_directory(TestBundles ${CMAKE_CURRENT_SOURCE_DIR}/bundles ${TEST_BUNDLES_DIR}
	DEPENDS TestBundle
	COMMANDS
		COMMAND ${CMAKE_COMMAND} -E make_directory ${TEST_BUNDLE9_CODE_DIR}
		COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:TestBundle> ${TEST_BUNDLE9_CODE_DIR}
)
add_dependencies(OSP-testrunner TestBundles)
