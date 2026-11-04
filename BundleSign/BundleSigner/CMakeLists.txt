# Sources
file(GLOB SRCS_G "src/*.cpp")
POCO_SOURCES_AUTO(SRCS ${SRCS_G})

# Version Resource
if(MSVC)
	source_group("Resources" FILES ${PROJECT_SOURCE_DIR}/DLLVersion.rc)
	list(APPEND SRCS ${PROJECT_SOURCE_DIR}/DLLVersion.rc)
endif()

poco_add_executable(BundleSigner ${SRCS})
set_target_properties(BundleSigner
	PROPERTIES
	OUTPUT_NAME signbundle
)

target_link_libraries(BundleSigner PRIVATE Poco::OSPBundleSign Poco::OSP Poco::Crypto Poco::Zip Poco::Util Poco::XML Poco::JSON Poco::Foundation)
poco_osp_link_applink(BundleSigner)

install(
	TARGETS BundleSigner
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime
)

if(POCO_OSP_ENABLE_TESTS)
	add_test(
		NAME OSPBundleSigner
		COMMAND ${CMAKE_COMMAND}
			-DSIGNBUNDLE=$<TARGET_FILE:BundleSigner>
			-DDATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../testsuite/data
			-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/SignBundleTest
			-P ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/SignBundleTest.cmake
	)
endif()
