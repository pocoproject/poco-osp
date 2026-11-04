set(TARGET "poco.data")
set(BUNDLE_SPEC "${CMAKE_CURRENT_SOURCE_DIR}/Data.bndlspec")

# Build Bundle
poco_osp_bundle(${TARGET}-bundle
	DEPENDS Poco::Data
	SPEC ${BUNDLE_SPEC}
	CODE $<TARGET_FILE:Poco::Data>
	DESTINATION ${POCO_OSP_BUNDLE_DIR}
	INSTALL_COMPONENT Bundles
)
