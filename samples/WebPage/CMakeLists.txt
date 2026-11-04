set(TARGET "com.appinf.osp.samples.webpage")
set(BUNDLE_SPEC "${CMAKE_CURRENT_SOURCE_DIR}/WebPage.bndlspec")

# Build Bundle (data only: the resources in bundle/, no library)
poco_osp_bundle(${TARGET}-bundle
	SPEC ${BUNDLE_SPEC}
	DESTINATION ${POCO_OSP_BUNDLE_DIR}
)

install(
	DIRECTORY	 ${POCO_OSP_BUNDLE_DIR}/
	DESTINATION  ${POCO_INSTALL_BUNDLEDIR}
	COMPONENT    Samples
	FILES_MATCHING PATTERN "${TARGET}*.bndl"
)
