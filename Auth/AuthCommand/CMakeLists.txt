set(TARGET "com.appinf.osp.auth.command")
set(BUNDLE_SPEC "${CMAKE_CURRENT_SOURCE_DIR}/AuthCommand.bndlspec")

# Sources
file(GLOB SRCS_G "src/*.cpp")
POCO_SOURCES_AUTO(SRCS ${SRCS_G})

# Dynamic Library
poco_osp_bundle_module(${TARGET} ${SRCS})

target_link_libraries(${TARGET}
	PRIVATE Poco::OSPAuth Poco::OSPShell Poco::OSP Poco::Zip Poco::Util Poco::XML Poco::JSON Poco::Foundation
)

# Build Bundle
poco_osp_bundle(${TARGET}-bundle
	DEPENDS ${TARGET}
	SPEC ${BUNDLE_SPEC}
	CODE $<TARGET_FILE:${TARGET}>
	DESTINATION ${POCO_OSP_BUNDLE_DIR}
	INSTALL_COMPONENT Bundles
)

if(POCO_OSP_ENABLE_TESTS)
	add_subdirectory(testsuite)
endif()
