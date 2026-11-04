# Sources
file(GLOB SRCS_G "src/*.cpp")
POCO_SOURCES_AUTO(SRCS ${SRCS_G})

# Version Resource
if(MSVC)
	source_group("Resources" FILES ${PROJECT_SOURCE_DIR}/DLLVersion.rc)
	list(APPEND SRCS ${PROJECT_SOURCE_DIR}/DLLVersion.rc)
endif()

poco_add_executable(StripBundle ${SRCS})
set_target_properties(StripBundle
	PROPERTIES
	OUTPUT_NAME stripbundle
)

target_link_libraries(StripBundle PRIVATE Poco::Zip Poco::Util Poco::XML Poco::JSON Poco::Foundation)
target_include_directories(StripBundle PRIVATE ${PROJECT_SOURCE_DIR}/BundleSign/src)

install(
	TARGETS StripBundle
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime
)

if(POCO_OSP_ENABLE_TESTS)
	add_test(
		NAME OSPStripBundle
		COMMAND ${CMAKE_COMMAND}
			-DSTRIPBUNDLE=$<TARGET_FILE:StripBundle>
			-DDATA_DIR=${PROJECT_SOURCE_DIR}/BundleSign/testsuite/data
			-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/StripBundleTest
			-P ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/StripBundleTest.cmake
	)
endif()
