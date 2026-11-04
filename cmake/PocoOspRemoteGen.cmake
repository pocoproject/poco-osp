# Copyright (c) 2025, Applied Informatics Software Engineering GmbH.
# and Contributors.
#
# Helper functions for OSP CMake consumers
#
# poco_remote_gen() runs the RemoteGenNG that the caller sets in POCO_REMOTEGEN_NG.
# find_package(Poco COMPONENTS OSPBundleCreator) includes this module from an installed poco-osp.

include_guard(GLOBAL)

#
# Generate source files with RemoteGenNG. CMake can't add
# sources to a target at compile time so they have to be generated before.
#
# poco_remote_gen(<remotegen_xml> GEN_DIR_NAME <name of the output directory> DEFINES <remotegen defines>)
#
# RemoteGenNG runs in the staging directory ${CMAKE_CURRENT_BINARY_DIR}/remotegen-<id>, which is emptied before it
# generates; a relative directory of the executable is relative to CMAKE_CURRENT_SOURCE_DIR. A completed generation
# removes the files of the previous one that it did not write and copies each changed file to the same relative path
# under CMAKE_CURRENT_BINARY_DIR; an output path that leaves the staging directory is written directly. A file that
# cannot be removed or copied stops the configure, and the next configure generates again. The configuration names its
# headers with the property CMAKE_CURRENT_SOURCE_DIR. With RemoteGen.output.osp.bundleActivator, every generation writes
# <Activator>Impl.cpp anew.
#
# GEN_DIR_NAME, relative to CMAKE_CURRENT_BINARY_DIR, is the directory that the configuration writes to; the configure
# stops when it is or contains the source or binary directory (top-level or current) or the directory of the
# configuration or of the file it resolves to, each as named and resolved. DEFINES are properties <name>=<value>, or
# <name> for an empty value. The call also passes CMAKE_CURRENT_SOURCE_DIR, POCO_OSP_BASE when it is defined, and
# RemoteGen.output.timestamps=false.
#
# A RemoteGenNG whose help names the option list-dependencies lists at every configure the headers that it reads and the
# directories that decide them; the call then generates only when the output directory, the completed generation of the
# call or a listed header is missing, or when the command, the list, the configuration, a listed header or the
# executable named with a directory changed since the start of that generation. The configuration and the listed paths
# are dependencies of the build. Otherwise RemoteGenNG generates at every configure, with a warning, and the
# configuration is the only dependency. A path that a CMake list or the build file cannot hold, or a listed directory
# that is or is in the build directory or the binary directory of the call, as named or resolved, is not a dependency; a
# warning names it.
#
function(poco_remote_gen REMOTEGEN_XML)

	set(singleValueArgs GEN_DIR_NAME)
	set(multiValueArgs DEFINES)

	cmake_parse_arguments(PARSE_ARGV 1 arg "" "${singleValueArgs}" "${multiValueArgs}")

	if ("${arg_GEN_DIR_NAME}" STREQUAL "")
		message(FATAL_ERROR "Missing parameter GEN_DIR_NAME")
	endif()

	# RemoteGenNG preprocesses the headers with the compiler of this id in its global configuration.
	if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
		set(_remotegen_compiler msvc)
	elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		set(_remotegen_compiler gcc)
	elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
		set(_remotegen_compiler clang)
	else()
		message(FATAL_ERROR "poco_remote_gen: RemoteGenNG has no compiler definition for ${CMAKE_CXX_COMPILER_ID}")
	endif()
	# RemoteGenNG runs on the build host, which decides its option syntax.
	if (CMAKE_HOST_WIN32)
		set(_remotegen_option /)
	else()
		set(_remotegen_option --)
	endif()
	set(REMOTEGEN_DEFINES ${_remotegen_option}compiler=${_remotegen_compiler})

	# RemoteGen.xml include paths into OSP use ${POCO_OSP_BASE}, so that they do not depend on POCO_BASE. The
	# source-tree check uses a file that is not installed, because an installation on Windows also has the OSP headers
	# next to the module directory.
	get_filename_component(_remotegen_osp_root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.." ABSOLUTE)
	if(NOT DEFINED POCO_OSP_BASE AND EXISTS "${_remotegen_osp_root}/cmake/PocoOSPSetup.cmake")
		set(POCO_OSP_BASE "${_remotegen_osp_root}")
	endif()
	set(_remotegen_properties ${arg_DEFINES})
	if(DEFINED POCO_OSP_BASE)
		list(APPEND _remotegen_properties POCO_OSP_BASE=${POCO_OSP_BASE})
	endif()
	list(APPEND _remotegen_properties CMAKE_CURRENT_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
		RemoteGen.output.timestamps=false)

	foreach (DEF ${_remotegen_properties})
		if (CMAKE_HOST_WIN32)
			list(APPEND REMOTEGEN_DEFINES /D ${DEF})
		else()
			list(APPEND REMOTEGEN_DEFINES -D${DEF})
		endif()
	endforeach()

	get_filename_component(_remotegen_xml "${REMOTEGEN_XML}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
	get_filename_component(_remotegen_gen_dir "${arg_GEN_DIR_NAME}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}")
	# One timestamp per output directory: its time is the start of the last generation, its content the command and the
	# list. The outputs file lists the files that the last generation wrote.
	string(MD5 _remotegen_gen_dir_id "${_remotegen_gen_dir}")
	set(REMOTEGEN_TIMESTAMP ${CMAKE_CURRENT_BINARY_DIR}/remotegen-${_remotegen_gen_dir_id}.timestamp)
	set(_remotegen_outputs_file ${CMAKE_CURRENT_BINARY_DIR}/remotegen-${_remotegen_gen_dir_id}.outputs)
	set(_remotegen_staging ${CMAKE_CURRENT_BINARY_DIR}/remotegen-${_remotegen_gen_dir_id})
	# A marker file in the directory shows whether it is or contains a kept directory. Its name differs per build
	# directory and run, so that configures can check one directory at the same time. A list would split a path with
	# a ;.
	if(EXISTS "${_remotegen_gen_dir}")
		string(RANDOM LENGTH 16 _remotegen_nonce)
		string(MD5 _remotegen_marker_id "${CMAKE_CURRENT_BINARY_DIR}/${_remotegen_nonce}")
		set(_remotegen_marker_name ".poco_remote_gen_${_remotegen_marker_id}")
		execute_process(COMMAND "${CMAKE_COMMAND}" -E touch "${_remotegen_gen_dir}/${_remotegen_marker_name}"
			RESULT_VARIABLE _remotegen_result
			OUTPUT_QUIET
			ERROR_QUIET)
		if(NOT _remotegen_result EQUAL 0)
			message(FATAL_ERROR "poco_remote_gen: the file ${_remotegen_marker_name} cannot be created in "
				"${_remotegen_gen_dir}, so the configure cannot check that GEN_DIR_NAME \"${arg_GEN_DIR_NAME}\" is "
				"not and does not contain a directory that is kept.")
		endif()
		get_filename_component(_remotegen_xml_dir "${_remotegen_xml}" DIRECTORY)
		file(REAL_PATH "${_remotegen_xml}" _remotegen_xml_real)
		get_filename_component(_remotegen_xml_real_dir "${_remotegen_xml_real}" DIRECTORY)
		set(_remotegen_contained "")
		foreach(_remotegen_kept IN ITEMS "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}"
				"${CMAKE_CURRENT_BINARY_DIR}" "${CMAKE_BINARY_DIR}" "${_remotegen_xml_dir}"
				"${_remotegen_xml_real_dir}")
			cmake_path(SET _remotegen_kept_named NORMALIZE "${_remotegen_kept}")
			file(REAL_PATH "${_remotegen_kept}" _remotegen_kept_real)
			foreach(_remotegen_form IN ITEMS "${_remotegen_kept_named}" "${_remotegen_kept_real}")
				set(_remotegen_dir "${_remotegen_form}")
				while(NOT EXISTS "${_remotegen_dir}/${_remotegen_marker_name}")
					cmake_path(GET _remotegen_dir PARENT_PATH _remotegen_parent)
					if(_remotegen_parent STREQUAL _remotegen_dir)
						break()
					endif()
					set(_remotegen_dir "${_remotegen_parent}")
				endwhile()
				if(EXISTS "${_remotegen_dir}/${_remotegen_marker_name}")
					set(_remotegen_contained "${_remotegen_form}")
					break()
				endif()
			endforeach()
			if(NOT _remotegen_contained STREQUAL "")
				break()
			endif()
		endforeach()
		file(REMOVE "${_remotegen_gen_dir}/${_remotegen_marker_name}")
		if(NOT _remotegen_contained STREQUAL "")
			message(FATAL_ERROR "poco_remote_gen: GEN_DIR_NAME \"${arg_GEN_DIR_NAME}\" names "
				"${_remotegen_gen_dir}, which is or contains ${_remotegen_contained}. Generated files are replaced and "
				"removed in it; name a directory that holds only the generated files.")
		endif()
	endif()

	# The help is read once per configure for each resolved RemoteGenNG command.
	if("${POCO_REMOTEGEN_NG}" STREQUAL "")
		message(FATAL_ERROR "poco_remote_gen: POCO_REMOTEGEN_NG is empty; set it to RemoteGenNG.")
	endif()
	list(GET POCO_REMOTEGEN_NG 0 _remotegen_executable)
	# A name without a directory is looked up on PATH.
	cmake_path(HAS_PARENT_PATH _remotegen_executable _remotegen_has_directory)
	if(_remotegen_has_directory)
		cmake_path(ABSOLUTE_PATH _remotegen_executable BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
	endif()
	set(_remotegen_run ${POCO_REMOTEGEN_NG})
	list(REMOVE_AT _remotegen_run 0)
	list(PREPEND _remotegen_run "${_remotegen_executable}")
	file(MAKE_DIRECTORY "${_remotegen_staging}")
	string(MD5 _remotegen_tool_id "${_remotegen_run}")
	get_property(_remotegen_lists GLOBAL PROPERTY _POCO_REMOTE_GEN_LISTS_${_remotegen_tool_id})
	if("${_remotegen_lists}" STREQUAL "")
		list(JOIN POCO_REMOTEGEN_NG " " _remotegen_command)
		execute_process(COMMAND ${_remotegen_run} ${_remotegen_option}help
			WORKING_DIRECTORY ${_remotegen_staging}
			RESULT_VARIABLE _remotegen_result
			OUTPUT_VARIABLE _remotegen_help
			ERROR_VARIABLE _remotegen_help)
		# A process that cannot start or that a signal ends gives a text instead of an exit status.
		if(NOT _remotegen_result MATCHES "^-?[0-9]+$")
			message(FATAL_ERROR "poco_remote_gen: the help run of RemoteGenNG ${_remotegen_command} did not end "
				"with an exit status: ${_remotegen_result}")
		endif()
		string(FIND "${_remotegen_help}" "list-dependencies" _remotegen_position)
		if(_remotegen_position EQUAL -1)
			set(_remotegen_lists FALSE)
			message(WARNING "poco_remote_gen: the help of RemoteGenNG ${_remotegen_command} does not name the option "
				"list-dependencies, so it runs at every configure, and a change of a header runs it only at the next "
				"configure.")
		else()
			set(_remotegen_lists TRUE)
		endif()
		set_property(GLOBAL PROPERTY _POCO_REMOTE_GEN_LISTS_${_remotegen_tool_id} ${_remotegen_lists})
	endif()

	# A CMake list, the Makefile check or the Ninja file changes or rejects a path with one of these characters.
	string(ASCII 128 _remotegen_byte_80)
	string(ASCII 255 _remotegen_byte_FF)
	if(CMAKE_HOST_WIN32)
		set(_remotegen_unchecked "[][;\"$|]|[^ -~${_remotegen_byte_80}-${_remotegen_byte_FF}]")
		set(_remotegen_unchecked_text "[, ], ;, \", $, | or a control character")
	else()
		set(_remotegen_unchecked "[][;\\\\\"$|]|[^ -~${_remotegen_byte_80}-${_remotegen_byte_FF}]")
		set(_remotegen_unchecked_text "[, ], ;, \\, \", $, | or a control character")
	endif()
	set(_remotegen_unchecked_paths "")
	set(_remotegen_build_paths "")
	if(_remotegen_xml MATCHES "${_remotegen_unchecked}")
		string(APPEND _remotegen_unchecked_paths "\n  ${_remotegen_xml}")
	else()
		set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_remotegen_xml}")
	endif()

	set(_needs_regen FALSE)
	if(NOT EXISTS "${_remotegen_gen_dir}" OR NOT EXISTS "${REMOTEGEN_TIMESTAMP}"
			OR "${_remotegen_xml}" IS_NEWER_THAN "${REMOTEGEN_TIMESTAMP}"
			OR (_remotegen_has_directory AND EXISTS "${_remotegen_executable}"
				AND "${_remotegen_executable}" IS_NEWER_THAN "${REMOTEGEN_TIMESTAMP}"))
		set(_needs_regen TRUE)
	endif()
	set(_remotegen_generation "${POCO_REMOTEGEN_NG};${REMOTEGEN_DEFINES};${_remotegen_xml}\n")
	if(_remotegen_lists)
		set(_remotegen_deps "${CMAKE_CURRENT_BINARY_DIR}/remotegen-${_remotegen_gen_dir_id}.deps")
		file(REMOVE "${_remotegen_deps}")
		execute_process(
			WORKING_DIRECTORY ${_remotegen_staging}
			COMMAND_ERROR_IS_FATAL ANY

			COMMAND ${_remotegen_run} ${REMOTEGEN_DEFINES} ${_remotegen_option}list-dependencies=${_remotegen_deps}
				${_remotegen_xml}
		)
		if(NOT EXISTS "${_remotegen_deps}")
			message(FATAL_ERROR "poco_remote_gen: the list run of RemoteGenNG for the configuration ${_remotegen_xml} "
				"exited with 0 but did not write the list ${_remotegen_deps}")
		endif()
		file(READ "${_remotegen_deps}" _remotegen_list)
		string(APPEND _remotegen_generation "${_remotegen_list}")
		if(NOT _needs_regen)
			file(READ "${REMOTEGEN_TIMESTAMP}" _remotegen_last_generation)
			if(NOT _remotegen_generation STREQUAL _remotegen_last_generation)
				set(_needs_regen TRUE)
			endif()
		endif()
		if(NOT _remotegen_list STREQUAL "" AND NOT _remotegen_list MATCHES "\n$")
			message(FATAL_ERROR "poco_remote_gen: the list ${_remotegen_deps} that RemoteGenNG wrote does not end with "
				"a line feed")
		endif()
		# The list is split as a string: a [, ] or ; in a path changes how a CMake list is split.
		set(_rest "${_remotegen_list}")
		while(NOT _rest STREQUAL "")
			string(FIND "${_rest}" "\n" _end)
			string(SUBSTRING "${_rest}" 0 ${_end} _line)
			math(EXPR _end "${_end} + 1")
			string(SUBSTRING "${_rest}" ${_end} -1 _rest)
			if(NOT _line MATCHES "^(file|directory) (.*)$")
				message(FATAL_ERROR "poco_remote_gen: the list ${_remotegen_deps} that RemoteGenNG wrote has a "
					"line that is neither file <path> nor directory <path>: ${_line}")
			endif()
			set(_path "${CMAKE_MATCH_2}")
			# IS_NEWER_THAN is also true for a file that does not exist.
			if(CMAKE_MATCH_1 STREQUAL "file" AND NOT _needs_regen AND "${_path}" IS_NEWER_THAN "${REMOTEGEN_TIMESTAMP}")
				set(_needs_regen TRUE)
				message(STATUS "RemoteGen source changed: ${_path}")
			endif()
			set(_remotegen_in_build FALSE)
			if(CMAKE_MATCH_1 STREQUAL "directory" AND NOT _path MATCHES "${_remotegen_unchecked}")
				file(REAL_PATH "${_path}" _remotegen_path_real)
				foreach(_remotegen_binary IN ITEMS "${CMAKE_BINARY_DIR}" "${CMAKE_CURRENT_BINARY_DIR}")
					file(REAL_PATH "${_remotegen_binary}" _remotegen_binary_real)
					cmake_path(IS_PREFIX _remotegen_binary "${_path}" NORMALIZE _remotegen_in_build)
					if(NOT _remotegen_in_build)
						cmake_path(IS_PREFIX _remotegen_binary_real "${_remotegen_path_real}" NORMALIZE
							_remotegen_in_build)
					endif()
					if(_remotegen_in_build)
						break()
					endif()
				endforeach()
			endif()
			if(_remotegen_in_build)
				string(APPEND _remotegen_build_paths "\n  ${_path}")
			elseif(_path MATCHES "${_remotegen_unchecked}")
				string(APPEND _remotegen_unchecked_paths "\n  ${_path}")
			else()
				set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_path}")
			endif()
		endwhile()
	else()
		set(_needs_regen TRUE)
	endif()
	if(NOT _remotegen_unchecked_paths STREQUAL "")
		message(WARNING "poco_remote_gen: these paths of the call with the configuration ${_remotegen_xml} are not "
			"dependencies of the build, because they have a ${_remotegen_unchecked_text}; a change of them runs "
			"RemoteGenNG only at the next configure:${_remotegen_unchecked_paths}")
	endif()
	if(NOT _remotegen_build_paths STREQUAL "")
		message(WARNING "poco_remote_gen: these listed directories of the call with the configuration "
			"${_remotegen_xml} are not dependencies of the build, because they are the build directory or the binary "
			"directory of the call or in one of them, into which a configure writes; a change of them runs RemoteGenNG "
			"only at the next configure:${_remotegen_build_paths}")
	endif()

	# Generate files in configure time to have the sources
	# available to be compiled for the target.

	if(_needs_regen)
		message(STATUS "Generate sources with RemoteGenNG in: ${_remotegen_gen_dir}")
		# Emptied before RemoteGenNG starts: the COMMANDs of one execute_process run at the same time.
		# The timestamp is written before the run, so that a header changed during it is newer, and renamed after
		# the run completes.
		file(REMOVE_RECURSE "${_remotegen_staging}")
		file(MAKE_DIRECTORY "${_remotegen_staging}")
		file(REMOVE "${REMOTEGEN_TIMESTAMP}")
		file(WRITE "${REMOTEGEN_TIMESTAMP}.new" "${_remotegen_generation}")
		execute_process(
			WORKING_DIRECTORY ${_remotegen_staging}
			COMMAND_ECHO STDOUT
			COMMAND_ERROR_IS_FATAL ANY

			COMMAND ${_remotegen_run} ${REMOTEGEN_DEFINES} ${_remotegen_xml}
		)
		# The glob pattern would take a [, ], * or ? of the staging path as a wildcard.
		string(REGEX REPLACE "([][*?])" "[\\1]" _remotegen_staging_pattern "${_remotegen_staging}")
		file(GLOB_RECURSE _remotegen_outputs LIST_DIRECTORIES false RELATIVE "${_remotegen_staging}"
			"${_remotegen_staging_pattern}/*")
		set(_remotegen_last_outputs "")
		if(EXISTS "${_remotegen_outputs_file}")
			# file(STRINGS) would end a path at each non-ASCII byte.
			file(READ "${_remotegen_outputs_file}" _remotegen_last_outputs)
			string(REPLACE "\n" ";" _remotegen_last_outputs "${_remotegen_last_outputs}")
		endif()
		# Both lists until the copy completes, so that a configure that stops in it keeps track of what it copied.
		set(_remotegen_tracked ${_remotegen_last_outputs} ${_remotegen_outputs})
		list(REMOVE_DUPLICATES _remotegen_tracked)
		list(JOIN _remotegen_tracked "\n" _remotegen_tracked)
		file(WRITE "${_remotegen_outputs_file}" "${_remotegen_tracked}")
		# Removed before the copy: on a file system that ignores the case, a stale output can name a new output.
		# An error of file() does not stop the function, which would record the generation as complete.
		foreach(_remotegen_output IN LISTS _remotegen_last_outputs)
			if(NOT _remotegen_output IN_LIST _remotegen_outputs)
				file(REMOVE "${CMAKE_CURRENT_BINARY_DIR}/${_remotegen_output}")
				if(EXISTS "${CMAKE_CURRENT_BINARY_DIR}/${_remotegen_output}")
					message(FATAL_ERROR "poco_remote_gen: ${CMAKE_CURRENT_BINARY_DIR}/${_remotegen_output}, which the "
						"last generation wrote and this one does not, cannot be removed; the next configure generates "
						"again.")
				endif()
			endif()
		endforeach()
		# An output with the same content keeps its time, so that it is not compiled again.
		foreach(_remotegen_output IN LISTS _remotegen_outputs)
			cmake_path(GET _remotegen_output PARENT_PATH _remotegen_output_dir)
			file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${_remotegen_output_dir}")
			file(COPY_FILE "${_remotegen_staging}/${_remotegen_output}"
				"${CMAKE_CURRENT_BINARY_DIR}/${_remotegen_output}" ONLY_IF_DIFFERENT RESULT _remotegen_result)
			if(NOT _remotegen_result EQUAL 0)
				message(FATAL_ERROR "poco_remote_gen: the generated file ${_remotegen_output} cannot be copied to "
					"${CMAKE_CURRENT_BINARY_DIR}: ${_remotegen_result}; the next configure generates again.")
			endif()
		endforeach()
		list(JOIN _remotegen_outputs "\n" _remotegen_outputs)
		file(WRITE "${_remotegen_outputs_file}" "${_remotegen_outputs}")
		file(RENAME "${REMOTEGEN_TIMESTAMP}.new" "${REMOTEGEN_TIMESTAMP}")
	else()
		message(VERBOSE "Generated sources are up to date: ${_remotegen_gen_dir}")
	endif()

endfunction()
