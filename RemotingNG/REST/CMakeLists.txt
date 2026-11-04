set(TARGET "com.appinf.osp.remotingng.rest")
set(BUNDLE_SPEC "${CMAKE_CURRENT_SOURCE_DIR}/REST.bndlspec")

# Sources
file(GLOB SRCS_G "src/*.cpp")
POCO_SOURCES_AUTO(SRCS ${SRCS_G})

# Dynamic Library
add_library(${TARGET} MODULE ${SRCS})
set_target_properties(${TARGET}
	PROPERTIES
	PREFIX ""
)

# On macOS, use .dylib instead of .so
if(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
	set_target_properties(${TARGET} PROPERTIES SUFFIX ".dylib")
endif()

target_link_libraries(
	${TARGET}
	PRIVATE
	Poco::RemotingNGREST Poco::OSP Poco::RemotingNG Poco::NetSSL
)

# Build Bundle
poco_osp_bundle(${TARGET}-bundle
	DEPENDS Poco::RemotingNGREST ${TARGET}
	SPEC ${BUNDLE_SPEC}
	CODE $<TARGET_FILE:Poco::RemotingNGREST> $<TARGET_FILE:${TARGET}>
	DESTINATION ${POCO_OSP_BUNDLE_DIR}
)

install(
	DIRECTORY	 ${POCO_OSP_BUNDLE_DIR}/
	DESTINATION  ${POCO_INSTALL_BUNDLEDIR}
	COMPONENT    Bundles
	FILES_MATCHING PATTERN "${TARGET}*.bndl"
)
