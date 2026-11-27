set(TARGET "com.appinf.osp.remotingng.rest")
set(BUNDLE_SPEC "${CMAKE_CURRENT_SOURCE_DIR}/REST.bndlspec")

# Sources
file(GLOB SRCS_G "src/*.cpp")
POCO_SOURCES_AUTO(SRCS ${SRCS_G})

# Dynamic Library
poco_osp_bundle_module(${TARGET} ${SRCS})

target_link_libraries(
	${TARGET}
	PRIVATE
	Poco::RemotingNGREST Poco::OSP Poco::RemotingNG Poco::NetSSL
)

# Build Bundle
poco_osp_bundle(${TARGET}-bundle
	DEPENDS Poco::RemotingNGREST ${TARGET}
	SPEC ${BUNDLE_SPEC}
	CODE $<TARGET_FILE:Poco::RemotingNGREST> $<TARGET_FILE:${TARGET}>
	DESTINATION ${POCO_OSP_BUNDLE_DIR}
	INSTALL_COMPONENT Bundles
)
