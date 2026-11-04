# Sources
file(GLOB SRCS_G "src/*.cpp")
POCO_SOURCES_AUTO(TEST_SRCS ${SRCS_G})

# Headers
file(GLOB_RECURSE HDRS_G "src/*.h")
POCO_HEADERS_AUTO(TEST_SRCS ${HDRS_G})

# The exporter and the collectors are part of the bundle library.
list(APPEND TEST_SRCS
	${CMAKE_CURRENT_SOURCE_DIR}/../src/MetricFamily.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/../src/StringExporter.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/../src/WebServerCollector.cpp
)

poco_add_executable(OSPPrometheus-testrunner ${TEST_SRCS})
target_include_directories(OSPPrometheus-testrunner PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
# The tests of the bundle activator load the bundle library.
add_dependencies(OSPPrometheus-testrunner poco.prometheus)
target_compile_definitions(OSPPrometheus-testrunner PRIVATE "PROMETHEUS_BUNDLE_LIBRARY=\"$<TARGET_FILE:poco.prometheus>\"")
add_test(
	NAME OSPPrometheus
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	COMMAND OSPPrometheus-testrunner -ignore ${POCO_OSP_CPPIGNORE_FILE} -all
)

target_link_libraries(OSPPrometheus-testrunner
	PRIVATE OSPTestSupport Poco::OSPWeb Poco::OSP Poco::Prometheus Poco::Zip Poco::Net Poco::Util Poco::XML
		Poco::Foundation Poco::CppUnit
)
