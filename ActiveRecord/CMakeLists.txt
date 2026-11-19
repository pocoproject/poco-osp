set(TARGET "poco.activerecord")
set(BUNDLE_SPEC "${CMAKE_CURRENT_SOURCE_DIR}/ActiveRecord.bndlspec")

# Build Bundle
poco_osp_bundle(${TARGET}-bundle
	DEPENDS Poco::ActiveRecord
	SPEC ${BUNDLE_SPEC}
	CODE $<TARGET_FILE:Poco::ActiveRecord>
	DESTINATION ${POCO_OSP_BUNDLE_DIR}
	INSTALL_COMPONENT Bundles
)
