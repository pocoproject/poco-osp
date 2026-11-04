set(TARGET "com.appinf.osp.mail.secure")
set(BUNDLE_SPEC "${CMAKE_CURRENT_SOURCE_DIR}/SecureMail.bndlspec")

# Sources
file(GLOB SRCS_G "src/*.cpp")
POCO_SOURCES_AUTO(SRCS ${SRCS_G})

# Headers
file(GLOB_RECURSE HDRS_G "src/*.h")
POCO_HEADERS_AUTO(SRCS ${HDRS_G})

# Dynamic Library
poco_osp_bundle_module(${TARGET} ${SRCS})

target_link_libraries(${TARGET} PRIVATE Poco::OSPMail Poco::OSP Poco::Zip Poco::NetSSL Poco::Net Poco::Util Poco::XML Poco::JSON Poco::Foundation)

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
