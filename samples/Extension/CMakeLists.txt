set(TARGET "com.appinf.osp.samples.extension")
set(BUNDLE_SPEC "${CMAKE_CURRENT_SOURCE_DIR}/Extension.bndlspec")

# Build Bundle (data only: the resources in bundle/, no library)
poco_osp_bundle(${TARGET}-bundle
	SPEC ${BUNDLE_SPEC}
	DESTINATION ${POCO_OSP_BUNDLE_DIR}
	INSTALL_COMPONENT Samples
)
