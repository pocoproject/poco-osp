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

# _poco_osp_write_file_list(<list_file> <file>...)
#
# Writes the list of the files to <list_file> when the list differs from its content. A command that
# depends on <list_file> as well as on the files runs again when a file is renamed or removed, which
# changes no remaining file (a renamed file also keeps its time).
function(_poco_osp_write_file_list LIST_FILE)
	file(WRITE ${LIST_FILE}.new "${ARGN}")
	file(COPY_FILE ${LIST_FILE}.new ${LIST_FILE} ONLY_IF_DIFFERENT)
endfunction()

# _poco_osp_copy_directory(<target> <source_dir> <destination_dir>
#                          [DEPENDS <dependencies>...]
#                          [COMMANDS COMMAND <command>... ...])
#
# Adds the target <target>, built by default, that replaces <destination_dir> with a copy of
# <source_dir> and then runs COMMANDS, which are COMMAND arguments of add_custom_command(). The copy
# is made again when a file of <source_dir> is added, changed, renamed or removed, or when one of
# DEPENDS changes.
function(_poco_osp_copy_directory COPY_TARGET COPY_SOURCE_DIR COPY_DESTINATION_DIR)
	cmake_parse_arguments(PARSE_ARGV 3 COPY "" "" "DEPENDS;COMMANDS")
	# A misspelt keyword would otherwise drop the values that follow it.
	if(DEFINED COPY_UNPARSED_ARGUMENTS)
		message(FATAL_ERROR "_poco_osp_copy_directory(${COPY_TARGET}): unknown arguments: ${COPY_UNPARSED_ARGUMENTS}")
	endif()

	file(GLOB_RECURSE _copy_files CONFIGURE_DEPENDS ${COPY_SOURCE_DIR}/*)
	set(_copy_list ${CMAKE_CURRENT_BINARY_DIR}/${COPY_TARGET}.content)
	_poco_osp_write_file_list(${_copy_list} ${_copy_files})
	set(_copied ${CMAKE_CURRENT_BINARY_DIR}/${COPY_TARGET}.copied)
	add_custom_command(
		OUTPUT ${_copied}
		COMMAND ${CMAKE_COMMAND} -E rm -rf ${COPY_DESTINATION_DIR}
		COMMAND ${CMAKE_COMMAND} -E copy_directory ${COPY_SOURCE_DIR} ${COPY_DESTINATION_DIR}
		${COPY_COMMANDS}
		COMMAND ${CMAKE_COMMAND} -E touch ${_copied}
		DEPENDS ${_copy_files} ${_copy_list} ${COPY_DEPENDS}
		VERBATIM
	)
	add_custom_target(${COPY_TARGET} ALL DEPENDS ${_copied})
endfunction()

# Add the library of a bundle
#
# poco_osp_bundle_module(<target_name> <source>...)
#
# Adds a MODULE library with the file name that OSP loads: the target name without a prefix, with
# the suffix .dylib on macOS.
function(poco_osp_bundle_module MODULE_TARGET)
	add_library(${MODULE_TARGET} MODULE ${ARGN})
	set_target_properties(${MODULE_TARGET} PROPERTIES PREFIX "")
	if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
		set_target_properties(${MODULE_TARGET} PROPERTIES SUFFIX ".dylib")
	endif()
endfunction()

# Create POCO OSP Bundle
#
# poco_osp_bundle(<target_name>
#                 SPEC <bundle_spec>
#                 DESTINATION <bundle_dir>
#                 [CODE <files>...]
#                 [DEPENDS <dependencies>...]
#                 [WORKING_DIRECTORY <directory>]
#                 [SYMLINKS]
#                 [INSTALL_COMPONENT <component>]
#                )
#
# <target_name>: The name of the generated target (e.g. MyLibBundle)
# SPEC: Path to the bundle spec file (e.g. ${CMAKE_CURRENT_SOURCE_DIR}/MyLibBundle.bndlspec)
# DESTINATION: Target directory where the bundle is created (e.g. ${CMAKE_CURRENT_BINARY_DIR}/bundles)
# CODE: The libraries the bundle contains, usually $<TARGET_FILE:...> of the libraries in DEPENDS; they
#   take the place of the <code> element of the spec. A library of this build given as $<TARGET_FILE:...>
#   gets the RPATH of the code cache; its target must be defined before the call.
# DEPENDS: Targets and files that need to be built before the bundle can be created, usually the
#   libraries in CODE; files listed here also create the bundle again when they change.
# WORKING_DIRECTORY: The directory of the paths in the spec and of its bundle/ directory, whose files
#   the bundle depends on; the directory of the spec by default.
# SYMLINKS: Symbolic links in the <files> of the spec are stored as links (BundleCreator --symlinks).
# INSTALL_COMPONENT: Installs the bundle file, <symbolic name>_<version>.bndl from the spec, into
#   ${POCO_INSTALL_BUNDLEDIR} (bundles if it is not set) with this install component.
function(poco_osp_bundle BUNDLE_TARGET)
	set(prefix "BUNDLE")
	set(options "SYMLINKS")
	set(oneValueArgs SPEC DESTINATION WORKING_DIRECTORY INSTALL_COMPONENT)
	set(multiValueArgs CODE DEPENDS)
	cmake_parse_arguments("${prefix}" "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

	# A misspelt keyword would otherwise drop the values that follow it.
	if(DEFINED BUNDLE_UNPARSED_ARGUMENTS)
		message(FATAL_ERROR "OSP bundle ${BUNDLE_TARGET}: unknown arguments: ${BUNDLE_UNPARSED_ARGUMENTS}")
	endif()

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

	# OSP extracts the libraries of all bundles into one directory, the code cache, so a library that
	# a bundle carries from this build looks for the others next to it, not in the build tree.
	# Directories outside the project, such as those of third-party libraries, stay in its RPATH.
	if(APPLE)
		set(_origin "@loader_path")
	elseif(UNIX)
		set(_origin "$ORIGIN")
	endif()
	foreach(_code IN LISTS BUNDLE_CODE)
		if(NOT _code MATCHES "^\\$<TARGET_FILE:([^>]+)>$")
			continue()
		endif()
		set(_code_target ${CMAKE_MATCH_1})
		# A target defined after the call would keep the RPATH of the build tree. Checked on every
		# platform, so that a project that works on one works on all.
		if(NOT TARGET ${_code_target})
			message(FATAL_ERROR "OSP bundle ${BUNDLE_TARGET}: ${_code_target} in CODE must be defined before "
				"poco_osp_bundle()")
		endif()
		if(NOT DEFINED _origin)
			continue()
		endif()
		get_target_property(_aliased ${_code_target} ALIASED_TARGET)
		if(_aliased)
			set(_code_target ${_aliased})
		endif()
		get_target_property(_imported ${_code_target} IMPORTED)
		get_target_property(_type ${_code_target} TYPE)
		if(NOT _imported AND _type MATCHES "^(SHARED|MODULE)_LIBRARY$")
			set_target_properties(${_code_target} PROPERTIES
				BUILD_WITH_INSTALL_RPATH ON
				INSTALL_RPATH "${_origin}"
				INSTALL_RPATH_USE_LINK_PATH ON
			)
		endif()
	endforeach()

	file(MAKE_DIRECTORY ${BUNDLE_DESTINATION})

	message(STATUS "OSP bundle: ${BUNDLE_TARGET}, ${OSP_OSNAME}/${OSP_OSARCH}, code: ${BUNDLE_CODE}, depends: ${BUNDLE_DEPENDS}")

	# BundleCreator runs on the build host, which decides its option syntax.
	if(CMAKE_HOST_WIN32)
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
	if(EXISTS ${BUNDLE_WORKING_DIRECTORY}/oci AND NOT CMAKE_HOST_WIN32)
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

	set(_CONTENT_LIST ${CMAKE_CURRENT_BINARY_DIR}/${BUNDLE_TARGET}.content)
	_poco_osp_write_file_list(${_CONTENT_LIST} ${BUNDLE_CONTENT_FILES})

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

	if(BUNDLE_INSTALL_COMPONENT)
		# The file name that BundleCreator writes: the symbolic name and the version of the manifest,
		# which precede its dependencies, with the version in the form of OSP's Version::toString().
		# A change of the spec configures again, so that the name follows the spec.
		set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${BUNDLE_SPEC})
		file(READ ${BUNDLE_SPEC} _spec)
		string(FIND "${_spec}" "<dependency>" _dependencies)
		string(SUBSTRING "${_spec}" 0 ${_dependencies} _spec)
		if(NOT _spec MATCHES "<symbolicName>[ \t\r\n]*([^< \t\r\n]+)[ \t\r\n]*</symbolicName>")
			message(FATAL_ERROR "OSP bundle: no symbolicName in ${BUNDLE_SPEC}")
		endif()
		set(_name "${CMAKE_MATCH_1}")
		if(NOT _spec MATCHES "<version>[ \t\r\n]*([0-9]+)\\.([0-9]+)\\.([0-9]+)(-[^< \t\r\n]+)?[ \t\r\n]*</version>")
			message(FATAL_ERROR "OSP bundle: no version <major>.<minor>.<revision>[-<designation>] in ${BUNDLE_SPEC}")
		endif()
		set(_designation "${CMAKE_MATCH_4}")
		math(EXPR _major "${CMAKE_MATCH_1}")
		math(EXPR _minor "${CMAKE_MATCH_2}")
		math(EXPR _revision "${CMAKE_MATCH_3}")
		set(_install_dir "${POCO_INSTALL_BUNDLEDIR}")
		if(NOT _install_dir)
			set(_install_dir "bundles")
		endif()
		install(
			FILES ${BUNDLE_DESTINATION}/${_name}_${_major}.${_minor}.${_revision}${_designation}.bndl
			DESTINATION ${_install_dir}
			COMPONENT ${BUNDLE_INSTALL_COMPONENT}
		)
	endif()

endfunction()