# Sources
file(GLOB SRCS_G "src/*.cpp")
POCO_SOURCES_AUTO(SRCS ${SRCS_G})

# Version Resource
if(MSVC)
	source_group("Resources" FILES ${PROJECT_SOURCE_DIR}/DLLVersion.rc)
	list(APPEND SRCS ${PROJECT_SOURCE_DIR}/DLLVersion.rc)
endif()

poco_add_executable(CodeCacheUtility ${SRCS})
set_target_properties(CodeCacheUtility
	PROPERTIES
	OUTPUT_NAME ccutil
)

target_link_libraries(CodeCacheUtility PRIVATE Poco::OSP Poco::Zip Poco::Util Poco::XML Poco::JSON Poco::Foundation)

install(
	TARGETS CodeCacheUtility
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime
)

if(POCO_OSP_ENABLE_TESTS)
	add_test(
		NAME OSPCodeCacheUtility
		COMMAND ${CMAKE_COMMAND}
			-DCCUTIL=$<TARGET_FILE:CodeCacheUtility>
			-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/CodeCacheUtilityTest
			-P ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/CodeCacheUtilityTest.cmake
	)
endif()
