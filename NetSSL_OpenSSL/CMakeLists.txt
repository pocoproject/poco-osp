set(TARGET "poco.net.ssl")
set(BUNDLE_SPEC "${CMAKE_CURRENT_SOURCE_DIR}/NetSSL_OpenSSL.bndlspec")

# Sources
file(GLOB SRCS_G "src/*.cpp")
POCO_SOURCES_AUTO(SRCS ${SRCS_G})

# Dynamic Library
poco_osp_bundle_module(${TARGET} ${SRCS})

target_link_libraries(${TARGET} PRIVATE Poco::OSP Poco::Zip Poco::NetSSL Poco::Crypto Poco::Net Poco::Util Poco::XML Poco::JSON Poco::Foundation)

# Build Bundle
poco_osp_bundle(${TARGET}-bundle
	DEPENDS Poco::NetSSL ${TARGET}
	SPEC ${BUNDLE_SPEC}
	CODE $<TARGET_FILE:Poco::NetSSL> $<TARGET_FILE:${TARGET}>
	DESTINATION ${POCO_OSP_BUNDLE_DIR}
	INSTALL_COMPONENT Bundles
)
