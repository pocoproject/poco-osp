#
# StripBundleTest.cmake
#
# Tests of the stripbundle tool.
#
# Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
# and Contributors.
#
# SPDX-License-Identifier:	BSL-1.0
#
# Variables: STRIPBUNDLE (the tool), DATA_DIR (test bundles), WORK_DIR (a
# directory for this test only, removed afterwards).
#

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/PocoOSPScriptTest.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../../BundleSign/testsuite/SymlinkBundleCheck.cmake")

# expect_contiguous_entries(<description> <file>)
# Checks that each local file header of a ZIP file starts where the previous
# entry ends and that the central directory follows the last entry, as a
# reader that does not use the central directory expects. The entries must
# not have data descriptors.
function(expect_contiguous_entries description path)
	file(READ "${path}" content HEX)
	string(LENGTH "${content}" length)
	set(offset 0)
	while(offset LESS length)
		string(SUBSTRING "${content}" ${offset} 8 signature)
		if(NOT signature STREQUAL "504b0304")
			break()
		endif()
		# Little-endian fields of the local file header: the compressed size at
		# byte 18, the lengths of the name and of the extra field at 26 and 28.
		math(EXPR sizeAt "${offset} + 36")
		math(EXPR lengthsAt "${offset} + 52")
		string(SUBSTRING "${content}" ${sizeAt} 8 size)
		string(SUBSTRING "${content}" ${lengthsAt} 8 lengths)
		string(REGEX REPLACE "(..)(..)(..)(..)" "0x\\4\\3\\2\\1" size "${size}")
		string(REGEX REPLACE "(..)(..)(..)(..)" "0x\\2\\1;0x\\4\\3" lengths "${lengths}")
		list(GET lengths 0 nameLength)
		list(GET lengths 1 extraLength)
		math(EXPR offset "${offset} + 2 * (30 + ${nameLength} + ${extraLength} + ${size})")
	endwhile()
	poco_osp_expect("${description}" signature STREQUAL "504b0102")
endfunction()

# read_central_directory(<file> <entries>)
# Sets <entries> to a list of <name>=<time> items, with the name and the
# modification time and date fields of each entry in the central directory of
# a ZIP file, in hex. The file must not have an archive comment.
function(read_central_directory path entriesVariable)
	file(READ "${path}" content HEX)
	string(LENGTH "${content}" length)
	# Little-endian fields of the end of central directory record, the last 22
	# bytes of the file: the number of entries at byte 10, the offset of the
	# central directory at byte 16.
	math(EXPR countAt "${length} - 24")
	math(EXPR offsetAt "${length} - 12")
	string(SUBSTRING "${content}" ${countAt} 4 count)
	string(SUBSTRING "${content}" ${offsetAt} 8 offset)
	string(REGEX REPLACE "(..)(..)" "0x\\2\\1" count "${count}")
	string(REGEX REPLACE "(..)(..)(..)(..)" "0x\\4\\3\\2\\1" offset "${offset}")
	math(EXPR count "${count}")
	math(EXPR offset "2 * ${offset}")
	set(entries "")
	while(count GREATER 0)
		# Little-endian fields of a central directory header: the modification
		# time and date at byte 12, the lengths of the name, of the extra field
		# and of the comment at 28, 30 and 32, the name at 46.
		math(EXPR timeAt "${offset} + 24")
		math(EXPR lengthsAt "${offset} + 56")
		math(EXPR nameAt "${offset} + 92")
		string(SUBSTRING "${content}" ${timeAt} 8 time)
		string(SUBSTRING "${content}" ${lengthsAt} 12 lengths)
		string(REGEX REPLACE "(..)(..)(..)(..)(..)(..)" "0x\\2\\1;0x\\4\\3;0x\\6\\5" lengths "${lengths}")
		list(GET lengths 0 nameLength)
		list(GET lengths 1 extraLength)
		list(GET lengths 2 commentLength)
		math(EXPR nameHexLength "2 * ${nameLength}")
		string(SUBSTRING "${content}" ${nameAt} ${nameHexLength} name)
		list(APPEND entries "${name}=${time}")
		math(EXPR offset "${offset} + 2 * (46 + ${nameLength} + ${extraLength} + ${commentLength})")
		math(EXPR count "${count} - 1")
	endwhile()
	set(${entriesVariable} "${entries}" PARENT_SCOPE)
endfunction()

# expect_times_kept(<description> <source> <file> <name>...)
# Checks that the named entries of a ZIP file have the modification time and
# date of the same entries in the source file.
function(expect_times_kept description source path)
	read_central_directory("${source}" sourceEntries)
	read_central_directory("${path}" entries)
	foreach(name IN LISTS ARGN)
		string(HEX "${name}" nameHex)
		set(sourceEntry ${sourceEntries})
		list(FILTER sourceEntry INCLUDE REGEX "^${nameHex}=")
		set(entry ${entries})
		list(FILTER entry INCLUDE REGEX "^${nameHex}=")
		poco_osp_expect("${description}: ${name}" sourceEntry AND entry STREQUAL sourceEntry)
	endforeach()
endfunction()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

poco_osp_expect_exit(${EXIT_NOINPUT} "strip a path that does not exist" "${STRIPBUNDLE}" missing.bndl)

# A file or directory that is not a bundle is left as it is.
file(WRITE "${WORK_DIR}/notes.bndlspec" "not a bundle\n")
poco_osp_expect_exit(${EXIT_DATAERR} "strip a file that is not a bundle" "${STRIPBUNDLE}" notes.bndlspec)
file(READ "${WORK_DIR}/notes.bndlspec" content)
poco_osp_expect("the file that is not a bundle is unchanged" content STREQUAL "not a bundle\n")
file(MAKE_DIRECTORY "${WORK_DIR}/tree/bin")
file(WRITE "${WORK_DIR}/tree/bin/tool" "tool\n")
poco_osp_expect_exit(${EXIT_DATAERR} "strip a directory that is not a bundle" "${STRIPBUNDLE}" tree)
poco_osp_expect("the bin directory of a directory that is not a bundle is kept" EXISTS "${WORK_DIR}/tree/bin/tool")

# A bundle file loses its bin entries; the other entries stay intact, also those with extra fields.
file(COPY_FILE "${DATA_DIR}/extra-field.bndl" "${WORK_DIR}/extra-field.bndl")
poco_osp_expect_exit(${EXIT_OK} "strip a bundle file" "${STRIPBUNDLE}" extra-field.bndl)
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar tf "${WORK_DIR}/extra-field.bndl"
	OUTPUT_VARIABLE listing
	COMMAND_ERROR_IS_FATAL ANY)
poco_osp_expect("the stripped bundle file has no bin entry" NOT listing MATCHES "bin/")
poco_osp_expect("the stripped bundle file keeps its manifest" listing MATCHES "META-INF/manifest.mf")
file(MAKE_DIRECTORY "${WORK_DIR}/extracted")
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xf "${WORK_DIR}/extra-field.bndl"
	WORKING_DIRECTORY "${WORK_DIR}/extracted"
	COMMAND_ERROR_IS_FATAL ANY)
file(READ "${WORK_DIR}/extracted/bundle.properties" content)
poco_osp_expect("a stripped entry with an extra field keeps its content"
	content STREQUAL "foo: bar\nbaz: compressible compressible compressible compressible\n")
expect_contiguous_entries("the entries of the stripped bundle file follow each other" "${WORK_DIR}/extra-field.bndl")
expect_times_kept("a stripped entry with an extra field keeps its modification time" "${DATA_DIR}/extra-field.bndl"
	"${WORK_DIR}/extra-field.bndl" META-INF/manifest.mf bundle.properties)

# The same for a bundle file whose directory entries have extra fields shorter than a ZIP64 extra field.
file(COPY_FILE "${DATA_DIR}/short-extra-field.bndl" "${WORK_DIR}/short-extra-field.bndl")
poco_osp_expect_exit(${EXIT_OK} "strip a bundle file with short extra fields" "${STRIPBUNDLE}" short-extra-field.bndl)
expect_contiguous_entries("the entries of the stripped bundle file with short extra fields follow each other"
	"${WORK_DIR}/short-extra-field.bndl")
file(MAKE_DIRECTORY "${WORK_DIR}/short-extracted")
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xf "${WORK_DIR}/short-extra-field.bndl"
	WORKING_DIRECTORY "${WORK_DIR}/short-extracted"
	COMMAND_ERROR_IS_FATAL ANY)
poco_osp_expect("the stripped bundle file with short extra fields has no bin entry"
	NOT EXISTS "${WORK_DIR}/short-extracted/bin")
file(READ "${WORK_DIR}/short-extracted/res/a.txt" content)
poco_osp_expect("a stripped entry with a short extra field keeps its content"
	content STREQUAL "hello hello hello hello\n")
expect_times_kept("a stripped entry with a short extra field keeps its modification time"
	"${DATA_DIR}/short-extra-field.bndl" "${WORK_DIR}/short-extra-field.bndl" META-INF/manifest.mf res/a.txt)

# A bundle file with an entry whose data does not match its CRC is left as it is.
file(COPY_FILE "${DATA_DIR}/corrupt-extra-field.bndl" "${WORK_DIR}/corrupt-extra-field.bndl")
poco_osp_expect_exit(${EXIT_DATAERR} "strip a bundle file with a corrupt entry" "${STRIPBUNDLE}"
	corrupt-extra-field.bndl)
file(SHA256 "${DATA_DIR}/corrupt-extra-field.bndl" originalHash)
file(SHA256 "${WORK_DIR}/corrupt-extra-field.bndl" hash)
poco_osp_expect("the bundle file with a corrupt entry is unchanged" hash STREQUAL originalHash)

# A bundle file written with --symlinks keeps its symbolic link and the mode of its executable file.
if(NOT WIN32)
	file(COPY_FILE "${DATA_DIR}/symlinks.bndl" "${WORK_DIR}/symlinks.bndl")
	poco_osp_expect_exit(${EXIT_OK} "strip a bundle file with a symbolic link" "${STRIPBUNDLE}" symlinks.bndl)
	expect_link_and_mode_kept("the stripped bundle file" "${WORK_DIR}/symlinks.bndl" "${WORK_DIR}/symlinks")
	poco_osp_expect("the stripped bundle file with a symbolic link has no bin entry"
		NOT EXISTS "${WORK_DIR}/symlinks/bin")
endif()

# A bundle file whose links or executables would lose their attributes, or with unknown attributes, is left as it is.
foreach(bundle symlink-extra-field.bndl symlink-fat-host-extra-field.bndl executable-extra-field.bndl
	central-name-mismatch.bndl)
	file(COPY_FILE "${DATA_DIR}/${bundle}" "${WORK_DIR}/${bundle}")
	poco_osp_expect_exit(${EXIT_DATAERR} "strip ${bundle}" "${STRIPBUNDLE}" ${bundle})
	file(SHA256 "${DATA_DIR}/${bundle}" originalHash)
	file(SHA256 "${WORK_DIR}/${bundle}" hash)
	poco_osp_expect("${bundle} is unchanged after stripping failed" hash STREQUAL originalHash)
endforeach()

# A bundle directory loses its bin directory.
file(MAKE_DIRECTORY "${WORK_DIR}/bundle/META-INF" "${WORK_DIR}/bundle/bin")
file(WRITE "${WORK_DIR}/bundle/META-INF/manifest.mf" "Manifest-Version: 1.0\n")
file(WRITE "${WORK_DIR}/bundle/bin/libx.so" "library\n")
poco_osp_expect_exit(${EXIT_OK} "strip a bundle directory" "${STRIPBUNDLE}" bundle)
poco_osp_expect("the bin directory of the bundle directory is removed" NOT EXISTS "${WORK_DIR}/bundle/bin")
poco_osp_expect("the manifest of the bundle directory is kept" EXISTS "${WORK_DIR}/bundle/META-INF/manifest.mf")

file(REMOVE_RECURSE "${WORK_DIR}")
