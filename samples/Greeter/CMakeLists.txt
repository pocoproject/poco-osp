set(TARGET "com.appinf.osp.samples.greeter")
set(BUNDLE_SPEC "${CMAKE_CURRENT_SOURCE_DIR}/Greeter.bndlspec")

# Sources
file(GLOB TARGET_SRCS_G "src/*.cpp")
POCO_SOURCES_AUTO(TARGET_SRCS ${TARGET_SRCS_G})

# Dynamic Library
add_library(${TARGET} MODULE ${TARGET_SRCS})
set_target_properties(${TARGET}
	PROPERTIES
	PREFIX ""
)

# On macOS, use .dylib instead of .so
if(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
	set_target_properties(${TARGET} PROPERTIES SUFFIX ".dylib")
endif()

target_link_libraries(${TARGET} PRIVATE Poco::OSP Poco::Zip Poco::Util Poco::XML Poco::JSON Poco::Foundation)
# The service interface is shared with the GreetingService sample.
target_include_directories(${TARGET}
	PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../GreetingService/include
)

# Build Bundle
poco_osp_bundle(${TARGET}-bundle
	DEPENDS ${TARGET}
	SPEC ${BUNDLE_SPEC}
	CODE $<TARGET_FILE:${TARGET}>
	DESTINATION ${POCO_OSP_BUNDLE_DIR}
)

install(
	DIRECTORY	 ${POCO_OSP_BUNDLE_DIR}/
	DESTINATION  ${POCO_INSTALL_BUNDLEDIR}
	COMPONENT    Samples
	FILES_MATCHING PATTERN "${TARGET}*.bndl"
)
