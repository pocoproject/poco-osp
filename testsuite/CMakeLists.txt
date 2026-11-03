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
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	COMMAND OSP-testrunner -ignore ${POCO_OSP_CPPIGNORE_FILE} -all
)
set_tests_properties(OSP PROPERTIES ENVIRONMENT POCO_BASE=${PROJECT_SOURCE_DIR})

target_link_libraries(OSP-testrunner PRIVATE Poco::OSPWebEvent Poco::OSP Poco::Zip Poco::Util Poco::XML Poco::Net Poco::JSON Poco::Foundation Poco::CppUnit)
target_include_directories(OSP-testrunner
	PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_library(TestBundle MODULE src/TestBundle.cpp)
set_target_properties(TestBundle
	PROPERTIES
	PREFIX ""
)

if(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
	set_target_properties(TestBundle PROPERTIES SUFFIX ".dylib")
endif()

target_link_libraries(TestBundle PRIVATE Poco::OSP Poco::Zip Poco::Util Poco::XML Poco::JSON Poco::Foundation)

add_custom_command(TARGET TestBundle
	POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_CURRENT_BINARY_DIR}/bundles
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/bundles ${CMAKE_CURRENT_BINARY_DIR}/bundles
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/bundles/com.appinf.osp.bundle9_1.0.0/bin/${OSP_OSNAME}/${OSP_OSARCH}
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:TestBundle> ${CMAKE_CURRENT_BINARY_DIR}/bundles/com.appinf.osp.bundle9_1.0.0/bin/${OSP_OSNAME}/${OSP_OSARCH}
)

# Build bundle2 .bndl file (output to temp location to avoid conflict with bundle directory)
poco_osp_bundle(TestBundle2
	SPEC ${CMAKE_CURRENT_SOURCE_DIR}/bundle2.bndlspec
	DESTINATION ${CMAKE_CURRENT_BINARY_DIR}
	DEPENDS TestBundle
)

add_custom_command(TARGET TestBundle2
	POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		${CMAKE_CURRENT_BINARY_DIR}/com.appinf.osp.bundle2_1.0.0.bndl
		${CMAKE_CURRENT_BINARY_DIR}/bundles/com.appinf.osp.bundle2_1.0.0.bndl
)
