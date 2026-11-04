# Sources
file(GLOB SRCS_G "src/*.cpp")
POCO_SOURCES_AUTO(TEST_SRCS ${SRCS_G})

# Headers
file(GLOB_RECURSE HDRS_G "src/*.h")
POCO_HEADERS_AUTO(TEST_SRCS ${HDRS_G})

# The service implementation is part of the bundle library, the fake SMTP
# server is the one of the Mail test suite, and the bundles for the activator
# are the ones of the Web test suite.
list(APPEND TEST_SRCS
	${CMAKE_CURRENT_SOURCE_DIR}/../src/SecureMailDeliveryServiceImpl.cpp
	${PROJECT_SOURCE_DIR}/Mail/testsuite/src/FakeSMTPServer.cpp
	${PROJECT_SOURCE_DIR}/Web/testsuite/src/TemporaryBundle.cpp
)

add_executable(OSPSecureMail-testrunner ${TEST_SRCS})
target_include_directories(OSPSecureMail-testrunner PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/../src
	${PROJECT_SOURCE_DIR}/Mail/testsuite/src
	${PROJECT_SOURCE_DIR}/Web/testsuite/src
)
# The test of the bundle activator loads the bundle library.
add_dependencies(OSPSecureMail-testrunner com.appinf.osp.mail.secure)
target_compile_definitions(OSPSecureMail-testrunner PRIVATE "SECUREMAIL_BUNDLE_LIBRARY=\"$<TARGET_FILE:com.appinf.osp.mail.secure>\"")
add_test(
	NAME OSPSecureMail
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	COMMAND OSPSecureMail-testrunner -ignore ${POCO_OSP_CPPIGNORE_FILE} -all
)
set_tests_properties(OSPSecureMail PROPERTIES ENVIRONMENT POCO_BASE=${PROJECT_SOURCE_DIR})

target_link_libraries(OSPSecureMail-testrunner PRIVATE Poco::OSPMail Poco::OSP Poco::NetSSL Poco::Crypto Poco::Net Poco::Zip Poco::Util Poco::XML Poco::Foundation Poco::CppUnit)
