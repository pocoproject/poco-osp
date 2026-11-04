# Sources
file(GLOB SRCS_G "src/*.cpp")
POCO_SOURCES_AUTO(TEST_SRCS ${SRCS_G})

# Headers
file(GLOB_RECURSE HDRS_G "src/*.h")
POCO_HEADERS_AUTO(TEST_SRCS ${HDRS_G})

add_executable(OSPWeb-testrunner ${TEST_SRCS})
if(ENABLE_TEST_DEPRECATED)
	target_compile_definitions(OSPWeb-testrunner PRIVATE POCO_TEST_DEPRECATED)
endif()
add_test(
	NAME OSPWeb
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	COMMAND OSPWeb-testrunner -ignore ${POCO_OSP_CPPIGNORE_FILE} -all
)
set_tests_properties(OSPWeb PROPERTIES ENVIRONMENT POCO_BASE=${PROJECT_SOURCE_DIR})

target_link_libraries(OSPWeb-testrunner PRIVATE Poco::OSPWeb Poco::OSP Poco::Zip Poco::Util Poco::XML Poco::Net Poco::JSON Poco::Foundation Poco::CppUnit)

# A library that the tests load: the tests of the private class LibraryReference, and the test
# that a bundle's library stays loaded until the session attributes it created are destroyed.
if(BUILD_SHARED_LIBS)
	add_library(OSPWebTestLibrary MODULE library/TestLibrary.cpp)
	target_link_libraries(OSPWebTestLibrary PRIVATE Poco::OSPWeb Poco::Foundation)
	add_dependencies(OSPWeb-testrunner OSPWebTestLibrary)
	target_include_directories(OSPWeb-testrunner PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
	target_compile_definitions(OSPWeb-testrunner PRIVATE "OSPWEB_TEST_LIBRARY=\"$<TARGET_FILE:OSPWebTestLibrary>\"")
endif()
