# Sources
file(GLOB SRCS_G "src/*.cpp")
POCO_SOURCES_AUTO(TEST_SRCS ${SRCS_G})

# Headers
file(GLOB_RECURSE HDRS_G "src/*.h")
POCO_HEADERS_AUTO(TEST_SRCS ${HDRS_G})

# The validator is part of the bundle library; the log capture is the one of
# the Web test suite.
list(APPEND TEST_SRCS
	${CMAKE_CURRENT_SOURCE_DIR}/../src/JWTValidator.cpp
	${PROJECT_SOURCE_DIR}/Web/testsuite/src/LogCapture.cpp
	${PROJECT_SOURCE_DIR}/Web/testsuite/src/TemporaryBundle.cpp
)

add_executable(OSPJWT-testrunner ${TEST_SRCS})
target_include_directories(OSPJWT-testrunner PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/../src
	${PROJECT_SOURCE_DIR}/Web/testsuite/src
)
# The test of the bundle activator loads the bundle library.
add_dependencies(OSPJWT-testrunner poco.jwt)
target_compile_definitions(OSPJWT-testrunner PRIVATE "JWT_BUNDLE_LIBRARY=\"$<TARGET_FILE:poco.jwt>\"")
add_test(
	NAME OSPJWT
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	COMMAND OSPJWT-testrunner -ignore ${POCO_OSP_CPPIGNORE_FILE} -all
)

target_link_libraries(OSPJWT-testrunner PRIVATE Poco::OSPWeb Poco::OSP Poco::JWT Poco::Crypto Poco::Zip Poco::Util Poco::XML Poco::JSON Poco::Foundation Poco::CppUnit)
