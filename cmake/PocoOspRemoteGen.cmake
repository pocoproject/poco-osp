# Copyright (c) 2025, Applied Informatics Software Engineering GmbH.
# and Contributors.
#
# Helper functions for OSP CMake consumers
#
# The superproject provides the RemoteGenNG tool (POCO_REMOTEGEN_NG) built at
# configure time; poco_remote_gen() below runs it.

include_guard(GLOBAL)

#
# Generate source files with RemoteGenNG. CMake can't add
# sources to a target at compile time so they have to be generated before.
# RemoteGenNG is also compiled separately at configure time for that purpose.
# (see above).
#
# poco_remote_gen(<remotegen_xml> GEN_DIR_NAME <name of the output directory> DEFINES <remotegen defines>)
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
	if (WIN32)
		set(REMOTEGEN_DEFINES /compiler=${_remotegen_compiler})
	else()
		set(REMOTEGEN_DEFINES --compiler=${_remotegen_compiler})
	endif()

	foreach (DEF ${arg_DEFINES})
		if (WIN32)
			list(APPEND REMOTEGEN_DEFINES /D ${DEF})
		else()
			list(APPEND REMOTEGEN_DEFINES -D${DEF})
		endif()
	endforeach()

	# RemoteGen.xml include paths into OSP use ${POCO_OSP_BASE}, so that they do not
	# depend on where POCO is (POCO_BASE). It defaults to the root of this repository;
	# superprojects with a different layout set POCO_OSP_BASE.
	if(NOT DEFINED POCO_OSP_BASE)
		get_filename_component(POCO_OSP_BASE "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.." ABSOLUTE)
	endif()
	if (WIN32)
		list(APPEND REMOTEGEN_DEFINES /D POCO_OSP_BASE=${POCO_OSP_BASE})
	else()
		list(APPEND REMOTEGEN_DEFINES -DPOCO_OSP_BASE=${POCO_OSP_BASE})
	endif()

	set(REMOTEGEN_TIMESTAMP ${CMAKE_CURRENT_BINARY_DIR}/remotegen.timestamp)
	file(TIMESTAMP ${REMOTEGEN_TIMESTAMP} CREATED_TIME)

	# Parse <include> entries from RemoteGen.xml to detect source header changes.
	file(READ ${REMOTEGEN_XML} _remotegen_xml_content)
	string(REGEX MATCHALL "<include>[^<]*</include>" _remotegen_include_block "${_remotegen_xml_content}")
	set(_remotegen_source_headers "")
	if(_remotegen_include_block)
		string(REGEX REPLACE "</?include>" "" _remotegen_include_content "${_remotegen_include_block}")
		string(REGEX MATCHALL "[^\n\r\t ]+" _remotegen_source_headers "${_remotegen_include_content}")
	endif()

	# Check if any source header is newer than the generation timestamp.
	set(_needs_regen FALSE)
	if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${arg_GEN_DIR_NAME})
		set(_needs_regen TRUE)
	else()
		file(TIMESTAMP ${REMOTEGEN_XML} XML_TIME)
		if(XML_TIME STRGREATER CREATED_TIME)
			set(_needs_regen TRUE)
		else()
			foreach(_hdr IN LISTS _remotegen_source_headers)
				if(EXISTS "${_hdr}")
					file(TIMESTAMP "${_hdr}" _hdr_time)
					if(_hdr_time STRGREATER CREATED_TIME)
						set(_needs_regen TRUE)
						message(STATUS "RemoteGen source changed: ${_hdr}")
						break()
					endif()
				endif()
			endforeach()
		endif()
	endif()

	# Generate files in configure time to have the sources
	# available to be compiled for the target.

	if(_needs_regen)
		message(STATUS "Generate sources with RemoteGenNG in: ${CMAKE_CURRENT_BINARY_DIR}")
		execute_process(
			WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
			COMMAND_ECHO STDOUT
			COMMAND_ERROR_IS_FATAL LAST

			COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_CURRENT_SOURCE_DIR}/${arg_GEN_DIR_NAME}
			COMMAND ${POCO_REMOTEGEN_NG} ${REMOTEGEN_DEFINES} ${REMOTEGEN_XML}
		)
		file(TOUCH ${REMOTEGEN_TIMESTAMP})
	else()
		message(NOTICE "Generated sources are up to date: ${CMAKE_CURRENT_BINARY_DIR}")
	endif()

	# Trigger reconfigure when RemoteGen.xml or any source header changes.
	set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${REMOTEGEN_XML})
	foreach(_hdr IN LISTS _remotegen_source_headers)
		if(EXISTS "${_hdr}")
			set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_hdr}")
		endif()
	endforeach()

endfunction()
