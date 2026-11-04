set(TARGET "com.appinf.osp.unit.testrunner")
set(BUNDLE_SPEC "${CMAKE_CURRENT_SOURCE_DIR}/TestRunner.bndlspec")

# Sources
file(GLOB SRCS_G "src/*.cpp")
POCO_SOURCES_AUTO(SRCS ${SRCS_G})

# Dynamic Library
poco_osp_bundle_module(${TARGET} ${SRCS})

target_link_libraries(${TARGET} PRIVATE Poco::OSPUnit Poco::OSP Poco::CppUnit Poco::XML Poco::Foundation)

# Build Bundle
poco_osp_bundle(${TARGET}-bundle
	DEPENDS Poco::OSPUnit Poco::CppUnit ${TARGET}
	SPEC ${BUNDLE_SPEC}
	CODE $<TARGET_FILE:Poco::OSPUnit> $<TARGET_FILE:Poco::CppUnit> $<TARGET_FILE:${TARGET}>
	DESTINATION ${POCO_OSP_BUNDLE_DIR}
	INSTALL_COMPONENT Bundles
)
