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

	if (WIN32)
		set(REMOTEGEN_DEFINES /compiler=msvc)
	else()
		set(REMOTEGEN_DEFINES --compiler=clang)
	endif()

	foreach (DEF ${arg_DEFINES})
		if (WIN32)
			list(APPEND REMOTEGEN_DEFINES /D ${DEF})
		else()
			list(APPEND REMOTEGEN_DEFINES -D${DEF})
		endif()
	endforeach()

	# Macchina-only RemoteGen.xml include paths use ${POCOPLUS_BASE} so that the
	# make build can keep vanilla POCO under POCO_BASE (the poco submodule). The make
	# build passes POCOPLUS_BASE via RemoteGen.sh; define it here for the CMake
	# codegen as well, pointing at the macchina platform source tree. Superprojects
	# with a different layout (standalone component builds) set POCOPLUS_BASE.
	if(NOT DEFINED POCOPLUS_BASE)
		set(POCOPLUS_BASE ${CMAKE_SOURCE_DIR}/macchina.io/platform)
	endif()
	if (WIN32)
		list(APPEND REMOTEGEN_DEFINES /D POCOPLUS_BASE=${POCOPLUS_BASE})
	else()
		list(APPEND REMOTEGEN_DEFINES -DPOCOPLUS_BASE=${POCOPLUS_BASE})
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
