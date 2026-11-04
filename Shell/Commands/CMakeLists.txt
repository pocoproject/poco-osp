set(TARGET "com.appinf.osp.shell.commands")
set(BUNDLE_SPEC "${CMAKE_CURRENT_SOURCE_DIR}/Commands.bndlspec")

# Sources
file(GLOB SRCS_G "src/*.cpp")
POCO_SOURCES_AUTO(SRCS ${SRCS_G})

# Headers
file(GLOB_RECURSE HDRS_G "src/*.h")
POCO_HEADERS_AUTO(SRCS ${HDRS_G})

# Dynamic Library
poco_osp_bundle_module(${TARGET} ${SRCS})

target_link_libraries(${TARGET} PRIVATE Poco::OSPShell Poco::OSP Poco::Util Poco::XML Poco::Foundation)

# Build Bundle
poco_osp_bundle(${TARGET}-bundle
	DEPENDS ${TARGET}
	SPEC ${BUNDLE_SPEC}
	CODE $<TARGET_FILE:${TARGET}>
	DESTINATION ${POCO_OSP_BUNDLE_DIR}
	INSTALL_COMPONENT Bundles
)
