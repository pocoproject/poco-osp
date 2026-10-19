set(TARGET "poco.redis")
set(BUNDLE_SPEC "${CMAKE_CURRENT_SOURCE_DIR}/Redis.bndlspec")

set(BUNDLE_DIR ${CMAKE_BINARY_DIR}/${POCO_INSTALL_BUNDLEDIR})

# Sources
file(GLOB SRCS "src/*.cpp")

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

target_link_libraries(${TARGET} Poco::OSP Poco::Zip Poco::Redis Poco::Util Poco::XML Poco::JSON Poco::Foundation)

# Build Bundle
poco_osp_bundle(${TARGET}-bundle
	DEPENDS ${LIBNAME} ${TARGET}
	SPEC ${BUNDLE_SPEC}
	CODE $<TARGET_FILE:Redis> $<TARGET_FILE:${TARGET}>
	DESTINATION ${BUNDLE_DIR}
)

install(
	DIRECTORY	 ${BUNDLE_DIR}/
	DESTINATION  ${POCO_INSTALL_BUNDLEDIR}
	COMPONENT    Bundles
	FILES_MATCHING PATTERN "${TARGET}*.bndl"
)
