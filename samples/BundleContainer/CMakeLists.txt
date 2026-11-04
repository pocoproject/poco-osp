# Sources
file(GLOB SAMPLE_SRCS "src/*.cpp")

poco_add_executable(BundleContainer ${SAMPLE_SRCS})

target_link_libraries(BundleContainer PRIVATE Poco::OSP Poco::Zip Poco::Util Poco::XML Poco::JSON Poco::Foundation)
# The program loads the bundles of the build, which include the bundles that use POCO Crypto and NetSSL.
poco_osp_link_applink(BundleContainer)

# The program reads a copy next to it, made again when the file changes.
add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/BundleContainer-files.copied
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/BundleContainer.properties
		$<TARGET_FILE_DIR:BundleContainer>
	COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/BundleContainer-files.copied
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/BundleContainer.properties
	VERBATIM
)
add_custom_target(BundleContainer-files ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/BundleContainer-files.copied)
