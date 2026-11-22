# A project that creates the bundle com.example.generated from one spec with poco_osp_bundle() three times, each
# into its own directory: plain without GENERATED, generated with GENERATED generated, and other with GENERATED naming
# another directory; and the bundle com.example.nested from the spec in nested into nested, with GENERATED
# generated/dist. Each configure appends a character to configure-count in the build directory.

cmake_minimum_required(VERSION 3.26)
project(BundleGeneratedTest NONE)

file(APPEND ${CMAKE_BINARY_DIR}/configure-count x)

include("${MODULE_DIR}/PocoOspBundle.cmake")

add_executable(Poco::OSP::BundleCreator IMPORTED)
set_target_properties(Poco::OSP::BundleCreator PROPERTIES IMPORTED_LOCATION "${BUNDLE_CREATOR}")

poco_osp_bundle(BundlePlain
	SPEC ${CMAKE_CURRENT_SOURCE_DIR}/generated.bndlspec
	DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/plain
)
# The directory with a trailing slash names the same directory.
poco_osp_bundle(BundleGenerated
	SPEC ${CMAKE_CURRENT_SOURCE_DIR}/generated.bndlspec
	DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/generated
	GENERATED generated/
)
poco_osp_bundle(BundleNested
	SPEC ${CMAKE_CURRENT_SOURCE_DIR}/nested/nested.bndlspec
	DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/nested
	GENERATED generated/dist
)
poco_osp_bundle(BundleOther
	SPEC ${CMAKE_CURRENT_SOURCE_DIR}/generated.bndlspec
	DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/other
	GENERATED maps
)
