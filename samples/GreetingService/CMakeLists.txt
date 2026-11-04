set(TARGET "com.appinf.osp.samples.greetingservice")
set(BUNDLE_SPEC "${CMAKE_CURRENT_SOURCE_DIR}/GreetingService.bndlspec")

# Sources
file(GLOB TARGET_SRCS_G "src/*.cpp")
POCO_SOURCES_AUTO(TARGET_SRCS ${TARGET_SRCS_G})

# Headers
file(GLOB_RECURSE TARGET_HDRS_G "include/*.h")
POCO_HEADERS_AUTO(TARGET_SRCS ${TARGET_HDRS_G})

# Dynamic Library
poco_osp_bundle_module(${TARGET} ${TARGET_SRCS})

target_link_libraries(${TARGET} PRIVATE Poco::OSP Poco::Zip Poco::Util Poco::XML Poco::JSON Poco::Foundation)
target_include_directories(${TARGET}
	PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Build Bundle
poco_osp_bundle(${TARGET}-bundle
	DEPENDS ${TARGET}
	SPEC ${BUNDLE_SPEC}
	CODE $<TARGET_FILE:${TARGET}>
	DESTINATION ${POCO_OSP_BUNDLE_DIR}
	INSTALL_COMPONENT Samples
)
