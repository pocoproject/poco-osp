set(TARGET "com.appinf.osp.samples.greeter")
set(BUNDLE_SPEC "${CMAKE_CURRENT_SOURCE_DIR}/Greeter.bndlspec")

set(BUNDLE_DIR ${CMAKE_BINARY_DIR}/${POCO_INSTALL_BUNDLEDIR})

# Sources
file(GLOB TARGET_SRCS_G "src/*.cpp")
POCO_SOURCES_AUTO(TARGET_SRCS ${TARGET_SRCS_G})

# Headers
file(GLOB_RECURSE TARGET_HDRS_G "include/*.h")
POCO_HEADERS_AUTO(TARGET_SRCS ${TARGET_HDRS_G})

# Dynamic Library
add_library(${TARGET} MODULE ${TARGET_SRCS})
set_target_properties(${TARGET}
	PROPERTIES
	PREFIX ""
)

# On macOS, use .dylib instead of .so
if(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
	set_target_properties(${TARGET} PROPERTIES SUFFIX ".dylib")
endif()

target_link_libraries(${TARGET} Poco::OSP Poco::Zip Poco::Util Poco::XML Poco::JSON Poco::Foundation)
target_include_directories(${TARGET}
	PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Build Bundle
poco_osp_bundle(${TARGET}-bundle
	DEPENDS ${LIBNAME} ${TARGET}
	SPEC ${BUNDLE_SPEC}
	CODE $<TARGET_FILE:${TARGET}>
	DESTINATION ${BUNDLE_DIR}
)

install(
	DIRECTORY	 ${BUNDLE_DIR}/
	DESTINATION  ${POCO_INSTALL_BUNDLEDIR}
	COMPONENT    Bundles
	FILES_MATCHING PATTERN "${TARGET}*.bndl"
)
