# A project that creates the bundle com.example.generated from one spec with poco_osp_bundle() four times, each into its
# own directory: plain without GENERATED, generated with GENERATED generated, dot with GENERATED ./generated, and other
# with GENERATED naming another directory; and the bundle com.example.nested from the spec in nested into nested, with
# GENERATED generated/dist; and the bundle com.example.written from the spec in written into written, with GENERATED
# app, which a writer command fills, and DEPENDS on the stamp of the writer. With CUSTOM_TARGET_WRITER, four more calls
# for the spec in written name a custom target of the writer in DEPENDS: alone, with the stamp, without GENERATED, and
# alone with the custom target defined after the call.
# With GENERATED_INVALID, one more call passes its value with GENERATED. Each configure appends a character to
# configure-count in the build directory.

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
poco_osp_bundle(BundleDot
	SPEC ${CMAKE_CURRENT_SOURCE_DIR}/generated.bndlspec
	DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/dot
	GENERATED ./generated
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

# The writer replaces the files of written/bundle/app with files of other names whenever app-version.txt changes.
set(stamp ${CMAKE_CURRENT_BINARY_DIR}/app.stamp)
add_custom_command(
	OUTPUT ${stamp}
	COMMAND ${CMAKE_COMMAND} -DVERSION_FILE=${CMAKE_CURRENT_SOURCE_DIR}/app-version.txt
		-DAPP_DIR=${CMAKE_CURRENT_SOURCE_DIR}/written/bundle/app -P ${CMAKE_CURRENT_SOURCE_DIR}/write-app.cmake
	COMMAND ${CMAKE_COMMAND} -E touch ${stamp}
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/app-version.txt ${CMAKE_CURRENT_SOURCE_DIR}/write-app.cmake
	VERBATIM
)
poco_osp_bundle(BundleWritten
	SPEC ${CMAKE_CURRENT_SOURCE_DIR}/written/written.bndlspec
	DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/written
	DEPENDS ${stamp}
	GENERATED app
)
if(CUSTOM_TARGET_WRITER)
	add_custom_target(WriteApp DEPENDS ${stamp})
	poco_osp_bundle(BundleTargetOnly
		SPEC ${CMAKE_CURRENT_SOURCE_DIR}/written/written.bndlspec
		DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/target-only
		DEPENDS WriteApp
		GENERATED app
	)
	poco_osp_bundle(BundleTargetAndStamp
		SPEC ${CMAKE_CURRENT_SOURCE_DIR}/written/written.bndlspec
		DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/target-and-stamp
		DEPENDS WriteApp ${stamp}
		GENERATED app
	)
	poco_osp_bundle(BundleTargetWithoutGenerated
		SPEC ${CMAKE_CURRENT_SOURCE_DIR}/written/written.bndlspec
		DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/target-without-generated
		DEPENDS WriteApp
	)
	poco_osp_bundle(BundleTargetLater
		SPEC ${CMAKE_CURRENT_SOURCE_DIR}/written/written.bndlspec
		DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/target-later
		DEPENDS WriteAppLater
		GENERATED app
	)
	add_custom_target(WriteAppLater DEPENDS ${stamp})
endif()
if(DEFINED GENERATED_INVALID)
	poco_osp_bundle(BundleInvalid
		SPEC ${CMAKE_CURRENT_SOURCE_DIR}/generated.bndlspec
		DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/invalid
		GENERATED ${GENERATED_INVALID}
	)
endif()
