set(TARGET "poco.redis")
set(BUNDLE_SPEC "${CMAKE_CURRENT_SOURCE_DIR}/Redis.bndlspec")

# Build Bundle
poco_osp_bundle(${TARGET}-bundle
	DEPENDS Poco::Redis
	SPEC ${BUNDLE_SPEC}
	CODE $<TARGET_FILE:Poco::Redis>
	DESTINATION ${POCO_OSP_BUNDLE_DIR}
	INSTALL_COMPONENT Bundles
)
