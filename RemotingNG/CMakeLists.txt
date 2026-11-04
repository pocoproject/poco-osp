set(TARGET "poco.remotingng")
set(BUNDLE_SPEC "${CMAKE_CURRENT_SOURCE_DIR}/RemotingNG.bndlspec")

# Build Bundle
poco_osp_bundle(${TARGET}-bundle
	DEPENDS Poco::RemotingNG
	SPEC ${BUNDLE_SPEC}
	CODE $<TARGET_FILE:Poco::RemotingNG>
	DESTINATION ${POCO_OSP_BUNDLE_DIR}
	INSTALL_COMPONENT Bundles
)
