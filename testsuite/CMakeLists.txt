# Sources
file(GLOB SRCS_G "src/*.cpp")
list(REMOVE_ITEM SRCS_G "${CMAKE_CURRENT_SOURCE_DIR}/src/TestBundle.cpp")
POCO_SOURCES_AUTO(TEST_SRCS ${SRCS_G})

# Headers
file(GLOB_RECURSE HDRS_G "src/*.h")
POCO_HEADERS_AUTO(TEST_SRCS ${HDRS_G})

add_executable(OSP-testrunner ${TEST_SRCS})
add_test(
	NAME OSP
	WORKING_DIRECTORY $<TARGET_FILE_DIR:OSP-testrunner>
	COMMAND OSP-testrunner -ignore ${POCO_OSP_CPPIGNORE_FILE} -all
)
set_tests_properties(OSP PROPERTIES
	ENVIRONMENT "POCO_BASE=${PROJECT_SOURCE_DIR};POCO_OSP_TEST_BUNDLES=${CMAKE_CURRENT_BINARY_DIR}/bundles"
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

add_library(TestBundle MODULE src/TestBundle.cpp)
set_target_properties(TestBundle
	PROPERTIES
	PREFIX ""
)

if(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
	set_target_properties(TestBundle PROPERTIES SUFFIX ".dylib")
endif()

target_link_libraries(TestBundle PRIVATE Poco::OSP Poco::Foundation)

# The tests need ENABLE_OSP_BUNDLES: bundle2 is created with BundleCreator.
# Created outside the copy of the bundles below, as BundleCreator replaces the
# directory of the bundle in its output directory.
file(GLOB_RECURSE TEST_BUNDLE2_FILES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/bundles/com.appinf.osp.bundle2_1.0.0/*)
poco_osp_bundle(TestBundle2
	SPEC ${CMAKE_CURRENT_SOURCE_DIR}/bundle2.bndlspec
	DESTINATION ${CMAKE_CURRENT_BINARY_DIR}
	DEPENDS ${TEST_BUNDLE2_FILES}
)

# The tests read the bundles from a copy in the build directory, which also holds
# the TestBundle library of bundle9 and the archive of bundle2.
file(GLOB_RECURSE TEST_BUNDLE_FILES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/bundles/*)
set(TEST_BUNDLES_DIR ${CMAKE_CURRENT_BINARY_DIR}/bundles)
set(TEST_BUNDLE9_CODE_DIR ${TEST_BUNDLES_DIR}/com.appinf.osp.bundle9_1.0.0/bin/${OSP_OSNAME}/${OSP_OSARCH})
add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/TestBundles.copied
	COMMAND ${CMAKE_COMMAND} -E rm -rf ${TEST_BUNDLES_DIR}
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/bundles ${TEST_BUNDLES_DIR}
	COMMAND ${CMAKE_COMMAND} -E make_directory ${TEST_BUNDLE9_CODE_DIR}
	COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:TestBundle> ${TEST_BUNDLE9_CODE_DIR}
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/com.appinf.osp.bundle2_1.0.0.bndl ${TEST_BUNDLES_DIR}
	COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/TestBundles.copied
	DEPENDS ${TEST_BUNDLE_FILES} TestBundle ${CMAKE_CURRENT_BINARY_DIR}/TestBundle2.created
	VERBATIM
)
add_custom_target(TestBundles ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/TestBundles.copied)
add_dependencies(TestBundles TestBundle2)
