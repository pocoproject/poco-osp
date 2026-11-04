set(TARGET "com.appinf.osp.samples.webinfo")
set(BUNDLE_SPEC "${CMAKE_CURRENT_SOURCE_DIR}/WebInfo.bndlspec")

# Sources
file(GLOB TARGET_SRCS "src/*.cpp")

# Dynamic Library
poco_osp_bundle_module(${TARGET} ${TARGET_SRCS})

target_link_libraries(${TARGET} PRIVATE Poco::OSPWeb Poco::OSP Poco::Zip Poco::Util Poco::XML Poco::JSON Poco::Foundation)

# Build Bundle
poco_osp_bundle(${TARGET}-bundle
	DEPENDS ${TARGET}
	SPEC ${BUNDLE_SPEC}
	CODE $<TARGET_FILE:${TARGET}>
	DESTINATION ${POCO_OSP_BUNDLE_DIR}
	INSTALL_COMPONENT Samples
)
