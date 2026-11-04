set(TARGET "osp.core")
set(BUNDLE_SPEC "${CMAKE_CURRENT_SOURCE_DIR}/Core.bndlspec")

# Build Bundle
poco_osp_bundle(${TARGET}-bundle
	SPEC ${BUNDLE_SPEC}
	DESTINATION ${POCO_OSP_BUNDLE_DIR}
	INSTALL_COMPONENT Bundles
)
