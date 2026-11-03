# Sources
file(GLOB SAMPLE_SRCS "src/*.cpp")

add_executable(BundleContainer ${SAMPLE_SRCS})

target_link_libraries(BundleContainer PRIVATE Poco::OSP Poco::Zip Poco::Util Poco::XML Poco::JSON Poco::Foundation)

add_custom_command(TARGET BundleContainer
	POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/BundleContainer.properties $<TARGET_FILE_DIR:BundleContainer>
)
