set(TARGET "com.appinf.osp.samples.greeter")
set(BUNDLE_SPEC "${CMAKE_CURRENT_SOURCE_DIR}/Greeter.bndlspec")

# Sources
file(GLOB TARGET_SRCS_G "src/*.cpp")
POCO_SOURCES_AUTO(TARGET_SRCS ${TARGET_SRCS_G})

# Dynamic Library
poco_osp_bundle_module(${TARGET} ${TARGET_SRCS})

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
	INSTALL_COMPONENT Samples
)
