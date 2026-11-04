# Copyright (c) 2021, Applied Informatics Software Engineering GmbH.
# and Contributors.
#
# Helper functions for OSP CMake consumers

include_guard(GLOBAL)

# Sets each of the two variables that is not defined to the platform of the
# build: the OS name and the processor architecture of the bundles.
function(_poco_osp_bundle_platform _osname_var _osarch_var)
	if(NOT DEFINED ${_osname_var})
		if (WIN32)
			set(${_osname_var} "Windows_NT" PARENT_SCOPE)
		else()
			set(${_osname_var} ${CMAKE_SYSTEM_NAME} PARENT_SCOPE)
		endif()
	endif()
	if(NOT DEFINED ${_osarch_var})
		set(${_osarch_var} ${CMAKE_SYSTEM_PROCESSOR} PARENT_SCOPE)
	endif()
endfunction()

# Set at include time and at directory scope, so that code outside poco_osp_bundle(), such as the
# core testsuite, uses the values the bundles are created with.
_poco_osp_bundle_platform(OSP_OSNAME OSP_OSARCH)

# Create Poco OSP Bundle
#
# poco_osp_bundle(<target_name>
#                 SPEC <bundle_spec>
#                 DESTINATION <bundle_dir>
#                 [DEPENDS <dependencies>...]
#                )
#
# <target_name>: The name of the generated target (e.g. MyLibBundle)
# SPEC: Path to the bundle spec file (e.g. ${CMAKE_CURRENT_SOURCE_DIR}/MyLibBundle.bndlspec)
# DESTINATION: Target directory where the bundle is created (e.g. ${CMAKE_CURRENT_BINARY_DIR}/bundles)
# DEPENDS: A list of targets that need to be build before the bundle can be created. Usually these are libraries created by add_library.
function(poco_osp_bundle BUNDLE_TARGET)
	set(prefix "BUNDLE")
	set(options "SYMLINKS")
	set(oneValueArgs SPEC DESTINATION WORKING_DIRECTORY)
	set(multiValueArgs CODE DEPENDS)
	cmake_parse_arguments("${prefix}" "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

	# A caller in a directory that does not see the include-time values gets local ones.
	_poco_osp_bundle_platform(OSP_OSNAME OSP_OSARCH)

	if (NOT BUNDLE_SPEC)
		message(FATAL_ERROR "OSP bundle: Missing argument SPEC")
	endif()

	if (NOT EXISTS ${BUNDLE_SPEC})
		message(FATAL_ERROR "OSP bundle: SPEC file does not exist: ${BUNDLE_SPEC}")
	endif()

	if (NOT BUNDLE_DESTINATION)
		message(FATAL_ERROR "OSP bundle: Missing argument DESTINATION")
	endif()

	if (NOT BUNDLE_WORKING_DIRECTORY)
		cmake_path(ABSOLUTE_PATH BUNDLE_SPEC NORMALIZE OUTPUT_VARIABLE _spec_absolute)
		cmake_path(GET _spec_absolute PARENT_PATH _spec_dir)
		set(BUNDLE_WORKING_DIRECTORY ${_spec_dir})
	endif()

	file(MAKE_DIRECTORY ${BUNDLE_DESTINATION})

	message(STATUS "OSP bundle: ${BUNDLE_TARGET}, ${OSP_OSNAME}/${OSP_OSARCH}, code: ${BUNDLE_CODE}, depends: ${BUNDLE_DEPENDS}")

	if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
		set(OPT_OUTPUT "/output")
		set(OPT_OSNAME "/osname")
		set(OPT_OSARCH "/osarch")
		set(OPT_CODE   "/code")
	else()
		set(OPT_OUTPUT "--output")
		set(OPT_OSNAME "--osname")
		set(OPT_OSARCH "--osarch")
		set(OPT_CODE   "--code")
	endif()

	set(BUNDLE_CREATED_FILE ${CMAKE_CURRENT_BINARY_DIR}/${BUNDLE_TARGET}.created)

	set(OPT_SYMLINKS "")
	if(BUNDLE_SYMLINKS)
		set(OPT_SYMLINKS "--symlinks")
	endif()

	# The bundle content (extensions.xml, bundle.properties, web resources, ...)
	# lives in the spec's bundle/ directory; without these dependencies, edits
	# there never re-trigger bundle creation and stale .bndl files get shipped.
	# The build checks the glob again, so that files added, renamed or removed
	# after configure are picked up.
	file(GLOB_RECURSE BUNDLE_CONTENT_FILES CONFIGURE_DEPENDS ${BUNDLE_WORKING_DIRECTORY}/bundle/*)

	# OCI container bundles additionally pack the spec's oci/ directory
	# (rootfs, config.json); track it too, or changes to the rootfs ship
	# stale .bndl files. The rootfs trees are large (>10k files) and
	# file(GLOB_RECURSE) has been observed to return incomplete,
	# cache-dependent results on them on some file systems, so instead of
	# file-level deps an always-run fingerprint step hashes the tree listing
	# at BUILD time and rewrites a stamp file only when the content actually
	# changed; the bundle command depends on that stamp. POSIX-only, like
	# the OCI container pipeline itself (the fingerprint uses find).
	set(_OCI_FINGERPRINT_TARGET "")
	if(EXISTS ${BUNDLE_WORKING_DIRECTORY}/oci AND NOT WIN32)
		set(_OCI_STAMP ${CMAKE_CURRENT_BINARY_DIR}/${BUNDLE_TARGET}-oci.stamp)
		set(_OCI_FINGERPRINT_TARGET ${BUNDLE_TARGET}-oci-fingerprint)
		add_custom_target(${_OCI_FINGERPRINT_TARGET}
			COMMAND ${CMAKE_COMMAND}
				-DOCI_DIR=${BUNDLE_WORKING_DIRECTORY}/oci
				-DSTAMP_FILE=${_OCI_STAMP}
				-P ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/PocoOspBundleFingerprint.cmake
			BYPRODUCTS ${_OCI_STAMP}
			COMMENT " - Fingerprint OCI content for ${BUNDLE_TARGET}"
			VERBATIM
		)
		list(APPEND BUNDLE_CONTENT_FILES ${_OCI_STAMP})
	endif()

	# A file renamed or removed changes no remaining file and a renamed file keeps its time, so the
	# bundle also depends on the list of its content files, which is rewritten only when it changes.
	set(_CONTENT_LIST ${CMAKE_CURRENT_BINARY_DIR}/${BUNDLE_TARGET}.content)
	file(WRITE ${_CONTENT_LIST}.new "${BUNDLE_CONTENT_FILES}")
	file(COPY_FILE ${_CONTENT_LIST}.new ${_CONTENT_LIST} ONLY_IF_DIFFERENT)

	add_custom_command(
		COMMENT " - Build OSP bundle in ${BUNDLE_DESTINATION} from ${BUNDLE_SPEC}"
		WORKING_DIRECTORY ${BUNDLE_WORKING_DIRECTORY}

		OUTPUT ${BUNDLE_CREATED_FILE}
		DEPENDS ${BUNDLE_DEPENDS} ${BUNDLE_SPEC} ${BUNDLE_CONTENT_FILES} ${_CONTENT_LIST}

		COMMAND Poco::OSP::BundleCreator ${OPT_SYMLINKS} ${OPT_OUTPUT} ${BUNDLE_DESTINATION} ${OPT_OSNAME} ${OSP_OSNAME} ${OPT_OSARCH} ${OSP_OSARCH} ${OPT_CODE} "${BUNDLE_CODE}" ${BUNDLE_SPEC}
		COMMAND ${CMAKE_COMMAND} -E touch ${BUNDLE_CREATED_FILE}
		VERBATIM
	)

	add_custom_target(${BUNDLE_TARGET} ALL DEPENDS ${BUNDLE_CREATED_FILE})
	# A consumer of an installed package has the imported Poco::OSP::BundleCreator but no BundleCreator build target.
	if(TARGET BundleCreator)
		add_dependencies(${BUNDLE_TARGET} BundleCreator)
	endif()
	if(_OCI_FINGERPRINT_TARGET)
		add_dependencies(${BUNDLE_TARGET} ${_OCI_FINGERPRINT_TARGET})
	endif()

endfunction()