#
# BundleCreatorTest.cmake
#
# Tests of the Bundle Creator tool (bundle).
#
# Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
# and Contributors.
#
# SPDX-License-Identifier:	BSL-1.0
#
# Variables: BUNDLE (the tool), WORK_DIR (a directory for this test only,
# removed afterwards).
#

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/PocoOSPScriptTest.cmake")

function(write_spec name symbolicName version files)
	file(WRITE "${WORK_DIR}/${name}.bndlspec"
		"<?xml version=\"1.0\"?>\n"
		"<bundlespec>\n"
		"  <manifest>\n"
		"    <name>Test Bundle</name>\n"
		"    <symbolicName>${symbolicName}</symbolicName>\n"
		"    <version>${version}</version>\n"
		"    <vendor>Test</vendor>\n"
		"  </manifest>\n"
		"  <files>${files}</files>\n"
		"</bundlespec>\n")
endfunction()

# Stores the names of the entries of a bundle file in the given variable.
function(list_bundle bundle variable)
	execute_process(COMMAND "${CMAKE_COMMAND}" -E tar tf "${bundle}"
		OUTPUT_VARIABLE listing
		COMMAND_ERROR_IS_FATAL ANY)
	set(${variable} "${listing}" PARENT_SCOPE)
endfunction()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

# The symbolic name and the version become file names in the output directory.
file(MAKE_DIRECTORY "${WORK_DIR}/out/inner" "${WORK_DIR}/out/victim_1.0.0")
file(WRITE "${WORK_DIR}/out/victim_1.0.0/important.txt" "keep\n")
write_spec(parent "../victim" "1.0.0" "")
poco_osp_expect_exit(FAILURE "symbolic name with a parent directory" "${BUNDLE}"
	${OPT}output=out/inner parent.bndlspec)
poco_osp_expect("the directory next to the output directory is kept" EXISTS "${WORK_DIR}/out/victim_1.0.0/important.txt")
poco_osp_expect("no bundle is written next to the output directory" NOT EXISTS "${WORK_DIR}/out/victim_1.0.0.bndl")
write_spec(slash "com.example/sub" "1.0.0" "")
poco_osp_expect_exit(FAILURE "symbolic name with a slash" "${BUNDLE}" ${OPT}output=out/inner slash.bndlspec)
write_spec(hidden ".example" "1.0.0" "")
poco_osp_expect_exit(FAILURE "symbolic name with a leading dot" "${BUNDLE}" ${OPT}output=out/inner hidden.bndlspec)
write_spec(designation "com.example.designation" "1.0.0-a/b" "")
poco_osp_expect_exit(FAILURE "version with a slash" "${BUNDLE}" ${OPT}output=out/inner designation.bndlspec)
write_spec(valid "com.example.valid_name" "1.0.0" "")
poco_osp_expect_exit(${EXIT_OK} "valid symbolic name" "${BUNDLE}" ${OPT}output=out/inner valid.bndlspec)
poco_osp_expect("the bundle with a valid symbolic name is written" EXISTS "${WORK_DIR}/out/inner/com.example.valid_name_1.0.0.bndl")

# A rebuilt bundle file contains only the files of the current build.
set(modes plain)
if(NOT WIN32)
	list(APPEND modes symlinks)
endif()
foreach(mode IN LISTS modes)
	set(options ${OPT}output=out/rebuild-${mode})
	if(mode STREQUAL "symlinks")
		list(APPEND options --symlinks)
	endif()
	file(REMOVE_RECURSE "${WORK_DIR}/res")
	file(MAKE_DIRECTORY "${WORK_DIR}/res/sub")
	file(WRITE "${WORK_DIR}/res/a.txt" "a\n")
	file(WRITE "${WORK_DIR}/res/sub/b.txt" "b\n")
	write_spec(rebuild "com.example.rebuild" "1.0.0" "res/*")
	set(bundle "${WORK_DIR}/out/rebuild-${mode}/com.example.rebuild_1.0.0.bndl")
	poco_osp_expect_exit(${EXIT_OK} "first build (${mode})" "${BUNDLE}" ${options} rebuild.bndlspec)
	list_bundle("${bundle}" listing)
	poco_osp_expect("the first build (${mode}) contains sub/b.txt" listing MATCHES "sub/b.txt")
	file(REMOVE_RECURSE "${WORK_DIR}/res/sub")
	poco_osp_expect_exit(${EXIT_OK} "second build (${mode})" "${BUNDLE}" ${options} rebuild.bndlspec)
	list_bundle("${bundle}" listing)
	poco_osp_expect("the second build (${mode}) does not contain the removed sub/b.txt" NOT listing MATCHES "sub/b.txt")
	poco_osp_expect("the second build (${mode}) contains a.txt" listing MATCHES "a.txt")
	file(GLOB outputFiles "${WORK_DIR}/out/rebuild-${mode}/*")
	poco_osp_expect("the output directory (${mode}) contains only the bundle file" outputFiles STREQUAL bundle)
endforeach()

# Files with an extension given with --no-deflate are stored without compression.
foreach(mode IN LISTS modes)
	set(options ${OPT}output=out/store-${mode} ${OPT}no-deflate=txt)
	if(mode STREQUAL "symlinks")
		list(APPEND options --symlinks)
	endif()
	file(REMOVE_RECURSE "${WORK_DIR}/res")
	file(MAKE_DIRECTORY "${WORK_DIR}/res")
	string(REPEAT "compressible text " 10000 text)
	file(WRITE "${WORK_DIR}/res/large.txt" "${text}")
	write_spec(store "com.example.store" "1.0.0" "res/*")
	poco_osp_expect_exit(${EXIT_OK} "build with --no-deflate=txt (${mode})" "${BUNDLE}" ${options} store.bndlspec)
	file(SIZE "${WORK_DIR}/out/store-${mode}/com.example.store_1.0.0.bndl" size)
	string(LENGTH "${text}" textSize)
	poco_osp_expect("the .txt file is stored without compression (${mode})" size GREATER textSize)
endforeach()

# Files are copied into the bundle by their name; two files with the same path are an error.
file(REMOVE_RECURSE "${WORK_DIR}/res")
file(MAKE_DIRECTORY "${WORK_DIR}/res/a" "${WORK_DIR}/res/b" "${WORK_DIR}/res/m1/html" "${WORK_DIR}/res/m2/html" "${WORK_DIR}/res/n1/html" "${WORK_DIR}/res/n2/html")
file(WRITE "${WORK_DIR}/res/a/x.txt" "A\n")
file(WRITE "${WORK_DIR}/res/b/x.txt" "B\n")
file(WRITE "${WORK_DIR}/res/m1/html/one.html" "1\n")
file(WRITE "${WORK_DIR}/res/m2/html/two.html" "2\n")
file(WRITE "${WORK_DIR}/res/n1/html/x.txt" "C\n")
file(WRITE "${WORK_DIR}/res/n2/html/x.txt" "D\n")
write_spec(duplicate "com.example.duplicate" "1.0.0" "res/a/x.txt, res/b/x.txt")
poco_osp_expect_exit(FAILURE "two files with the same name" "${BUNDLE}" ${OPT}output=out/duplicate duplicate.bndlspec)
write_spec(merged "com.example.merged" "1.0.0" "res/m1/html, res/m2/html")
poco_osp_expect_exit(${EXIT_OK} "two directories with the same name" "${BUNDLE}"
	${OPT}output=out/merged merged.bndlspec)
list_bundle("${WORK_DIR}/out/merged/com.example.merged_1.0.0.bndl" listing)
poco_osp_expect("the merged directory contains the files of both directories" listing MATCHES "html/one.html" AND listing MATCHES "html/two.html")
write_spec(nested "com.example.nested" "1.0.0" "res/n1/html, res/n2/html")
poco_osp_expect_exit(FAILURE "two files with the same path in merged directories" "${BUNDLE}"
	${OPT}output=out/nested nested.bndlspec)

# With --symlinks, links are stored as links, also dangling ones, and hidden links are skipped like hidden files.
if(NOT WIN32)
	file(REMOVE_RECURSE "${WORK_DIR}/res")
	file(MAKE_DIRECTORY "${WORK_DIR}/res/links" "${WORK_DIR}/res/code")
	file(WRITE "${WORK_DIR}/res/links/target.txt" "target\n")
	file(CREATE_LINK target.txt "${WORK_DIR}/res/links/link" SYMBOLIC)
	file(CREATE_LINK target.txt "${WORK_DIR}/res/links/.hidden-link" SYMBOLIC)
	file(CREATE_LINK missing.txt "${WORK_DIR}/res/links/dangling" SYMBOLIC)
	file(CREATE_LINK libmissing.so "${WORK_DIR}/res/code/libdangling.so" SYMBOLIC)
	write_spec(links "com.example.links" "1.0.0" "res/links")
	poco_osp_expect_exit(${EXIT_OK} "build with --symlinks" "${BUNDLE}" -o out/links --symlinks --code=res/code/libdangling.so links.bndlspec)
	set(bundle "${WORK_DIR}/out/links/com.example.links_1.0.0.bndl")
	list_bundle("${bundle}" listing)
	poco_osp_expect("the bundle contains the link" listing MATCHES "links/link")
	poco_osp_expect("the bundle contains the dangling link" listing MATCHES "links/dangling")
	poco_osp_expect("the bundle does not contain the hidden link" NOT listing MATCHES "hidden-link")
	poco_osp_expect("the bundle contains the dangling link given as code" listing MATCHES "libdangling.so")
	file(MAKE_DIRECTORY "${WORK_DIR}/extracted")
	execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xf "${bundle}"
		WORKING_DIRECTORY "${WORK_DIR}/extracted"
		COMMAND_ERROR_IS_FATAL ANY)
	poco_osp_expect("the link is extracted as a symbolic link" IS_SYMLINK "${WORK_DIR}/extracted/links/link")
endif()

file(REMOVE_RECURSE "${WORK_DIR}")
