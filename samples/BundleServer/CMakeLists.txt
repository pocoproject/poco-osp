# Sources
file(GLOB SAMPLE_SRCS "src/*.cpp")

poco_add_executable(BundleServer ${SAMPLE_SRCS})

target_link_libraries(BundleServer PRIVATE Poco::OSP Poco::Zip Poco::Util Poco::XML Poco::JSON Poco::Foundation)
# The program loads the bundles of the build, which include the bundles that use POCO Crypto and NetSSL.
poco_osp_link_applink(BundleServer)

# The program reads copies next to it, made again when a file changes.
add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/BundleServer-files.copied
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/BundleServer.properties
		${CMAKE_CURRENT_SOURCE_DIR}/any.pem $<TARGET_FILE_DIR:BundleServer>
	COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/BundleServer-files.copied
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/BundleServer.properties ${CMAKE_CURRENT_SOURCE_DIR}/any.pem
	VERBATIM
)
add_custom_target(BundleServer-files ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/BundleServer-files.copied)
