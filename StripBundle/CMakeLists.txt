# Sources
file(GLOB SRCS_G "src/*.cpp")
POCO_SOURCES_AUTO(SRCS ${SRCS_G})

# Version Resource
if(MSVC)
	source_group("Resources" FILES ${PROJECT_SOURCE_DIR}/DLLVersion.rc)
	list(APPEND SRCS ${PROJECT_SOURCE_DIR}/DLLVersion.rc)
endif()

add_executable(StripBundle ${SRCS})
set_target_properties(StripBundle
	PROPERTIES
	OUTPUT_NAME stripbundle
)

target_link_libraries(StripBundle PRIVATE Poco::Zip Poco::Util Poco::XML Poco::JSON Poco::Foundation)

install(
	TARGETS StripBundle
	RUNTIME DESTINATION bin COMPONENT Runtime
)
