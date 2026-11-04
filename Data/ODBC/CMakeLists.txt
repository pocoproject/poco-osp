set(TARGET "poco.data.odbc")
set(BUNDLE_SPEC "${CMAKE_CURRENT_SOURCE_DIR}/ODBC.bndlspec")

# Sources
file(GLOB SRCS_G "src/*.cpp")
POCO_SOURCES_AUTO(SRCS ${SRCS_G})

# Dynamic Library
poco_osp_bundle_module(${TARGET} ${SRCS})

target_link_libraries(${TARGET} PRIVATE Poco::OSP Poco::Zip Poco::DataODBC Poco::Data Poco::Util Poco::XML Poco::JSON Poco::Foundation)

# Build Bundle
poco_osp_bundle(${TARGET}-bundle
	DEPENDS Poco::DataODBC ${TARGET}
	SPEC ${BUNDLE_SPEC}
	CODE $<TARGET_FILE:Poco::DataODBC> $<TARGET_FILE:${TARGET}>
	DESTINATION ${POCO_OSP_BUNDLE_DIR}
	INSTALL_COMPONENT Bundles
)
