set(TARGET "com.appinf.osp.samples.webpage")
set(BUNDLE_SPEC "${CMAKE_CURRENT_SOURCE_DIR}/WebPage.bndlspec")

# Build Bundle (data only: the resources in bundle/, no library)
poco_osp_bundle(${TARGET}-bundle
	SPEC ${BUNDLE_SPEC}
	DESTINATION ${POCO_OSP_BUNDLE_DIR}
	INSTALL_COMPONENT Samples
)
