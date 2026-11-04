# Sources
file(GLOB SRCS_G "src/*.cpp")
POCO_SOURCES_AUTO(TEST_SRCS ${SRCS_G})

# Headers
file(GLOB_RECURSE HDRS_G "src/*.h")
POCO_HEADERS_AUTO(TEST_SRCS ${HDRS_G})

poco_add_executable(OSPSamples-testrunner ${TEST_SRCS})
add_test(
	NAME OSPSamples
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	COMMAND OSPSamples-testrunner -ignore ${POCO_OSP_CPPIGNORE_FILE} -all
)
set_tests_properties(OSPSamples PROPERTIES ENVIRONMENT POCO_BASE=${PROJECT_SOURCE_DIR})

# The interface of the GreetingService sample
target_include_directories(OSPSamples-testrunner PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../GreetingService/include)

target_link_libraries(OSPSamples-testrunner
	PRIVATE OSPTestSupport OSPTestSupportNet Poco::OSPWebEvent Poco::OSPWeb Poco::OSP Poco::Zip Poco::Util Poco::XML
		Poco::Net Poco::JSON Poco::Foundation Poco::CppUnit
)

# The tests run the bundles of the build, in the test process and with the sample programs.
target_compile_definitions(OSPSamples-testrunner PRIVATE
	"OSP_SAMPLES_BUNDLE_DIR=\"${POCO_OSP_BUNDLE_DIR}\""
	"OSP_SAMPLES_BUNDLE_SERVER=\"$<TARGET_FILE:BundleServer>\""
	"OSP_SAMPLES_BUNDLE_CONTAINER=\"$<TARGET_FILE:BundleContainer>\""
)
add_dependencies(OSPSamples-testrunner
	BundleServer
	BundleContainer
	poco.net-bundle
	osp.web-bundle
	osp.web.server-bundle
	com.appinf.osp.webevent-bundle
	com.appinf.osp.samples.auth-bundle
	com.appinf.osp.samples.extensible-bundle
	com.appinf.osp.samples.extension-bundle
	com.appinf.osp.samples.greetingservice-bundle
	com.appinf.osp.samples.preferences-bundle
	com.appinf.osp.samples.webecho-bundle
	com.appinf.osp.samples.webevent-bundle
	com.appinf.osp.samples.webindex-bundle
	com.appinf.osp.samples.webinfo-bundle
	com.appinf.osp.samples.webpage-bundle
	com.appinf.osp.samples.websession-bundle
)
# The sample programs read their default configuration next to them; the tests check that --config takes precedence.
add_dependencies(OSPSamples-testrunner BundleServer-files BundleContainer-files)

# A test runs the SimpleAuthService sample beside the SimpleAuth bundle, which has a service class of the same name.
if(ENABLE_OSP_SIMPLEAUTH)
	add_dependencies(OSPSamples-testrunner com.appinf.osp.simple-auth-bundle)
	target_compile_definitions(OSPSamples-testrunner PRIVATE OSP_SAMPLES_TEST_SIMPLE_AUTH=1)
endif()

add_test(
	NAME OSPSamplesInstall
	COMMAND ${CMAKE_COMMAND}
		-DBUILD_DIR=${CMAKE_BINARY_DIR}
		-DCONFIG=$<CONFIG>
		-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/SamplesInstallTest
		-P ${CMAKE_CURRENT_SOURCE_DIR}/SamplesInstallTest.cmake
)

add_test(
	NAME OSPSamplesBundleContent
	COMMAND ${CMAKE_COMMAND}
		-DBUNDLE_CREATOR=$<TARGET_FILE:BundleCreator>
		-DCMAKE_DIR=${PROJECT_SOURCE_DIR}/cmake
		-DGENERATOR=${CMAKE_GENERATOR}
		-DMAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
		-DCONFIG=$<CONFIG>
		-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/BundleContentTest
		-P ${CMAKE_CURRENT_SOURCE_DIR}/BundleContentTest.cmake
)
