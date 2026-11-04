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
write_spec(spaced " com.example.spaced " " 1.0.0 " "")
poco_osp_expect_exit(${EXIT_OK} "symbolic name and version with surrounding spaces" "${BUNDLE}"
	${OPT}output=out/inner spaced.bndlspec)
poco_osp_expect("the bundle is named without the spaces" EXISTS "${WORK_DIR}/out/inner/com.example.spaced_1.0.0.bndl")

# The platform of a <code> element, or --osname and --osarch, name two directories in the bin directory.
function(write_code_spec name platform)
	file(WRITE "${WORK_DIR}/${name}.bndlspec"
		"<?xml version=\"1.0\"?>\n"
		"<bundlespec>\n"
		"  <manifest>\n"
		"    <name>Test Bundle</name>\n"
		"    <symbolicName>com.example.${name}</symbolicName>\n"
		"    <version>1.0.0</version>\n"
		"    <vendor>Test</vendor>\n"
		"  </manifest>\n"
		"  <code platform=\"${platform}\">code/libx.so</code>\n"
		"</bundlespec>\n")
endfunction()
file(MAKE_DIRECTORY "${WORK_DIR}/code")
file(WRITE "${WORK_DIR}/code/libx.so" "library\n")
write_code_spec(escape "x/../../../../../escaped")
poco_osp_expect_exit(FAILURE "platform with a parent directory" "${BUNDLE}" ${OPT}output=out/platform escape.bndlspec)
poco_osp_expect("no file is written outside the output directory" NOT EXISTS "${WORK_DIR}/escaped")
write_code_spec(nested "Linux/x86_64/extra")
poco_osp_expect_exit(FAILURE "platform with three components" "${BUNDLE}" ${OPT}output=out/platform nested.bndlspec)
write_code_spec(platform "Linux/x86_64")
poco_osp_expect_exit(${EXIT_OK} "platform with two components" "${BUNDLE}" ${OPT}output=out/platform platform.bndlspec)
list_bundle("${WORK_DIR}/out/platform/com.example.platform_1.0.0.bndl" listing)
poco_osp_expect("the code is in bin/<osname>/<osarch>" listing MATCHES "bin/Linux/x86_64/libx.so")
write_code_spec(default "")
poco_osp_expect_exit(${EXIT_OK} "--osname and --osarch with path characters" "${BUNDLE}"
	${OPT}output=out/platform ${OPT}osname=../.. ${OPT}osarch=a/b default.bndlspec)
list_bundle("${WORK_DIR}/out/platform/com.example.default_1.0.0.bndl" listing)
poco_osp_expect("the path characters of --osname and --osarch are replaced" listing MATCHES "bin/_____/a_b/libx.so")
# The two bytes of U+00E9 in UTF-8, written with string(ASCII) to keep this file ASCII.
string(ASCII 195 169 eAcute)
write_code_spec(nonascii "")
poco_osp_expect_exit(${EXIT_OK} "--osname with a character outside ASCII" "${BUNDLE}"
	${OPT}output=out/platform ${OPT}osname=x${eAcute}y ${OPT}osarch=arch nonascii.bndlspec)
list_bundle("${WORK_DIR}/out/platform/com.example.nonascii_1.0.0.bndl" listing)
poco_osp_expect("each byte of a character outside ASCII in --osname is replaced"
	listing MATCHES "bin/x__y/arch/libx.so")
# The two names are not parsed as a path: "~" is a directory in the bin directory, not the home directory.
set(home "$ENV{HOME}")
set(ENV{HOME} "${WORK_DIR}/home")
write_code_spec(tilde "~/escaped")
poco_osp_expect_exit(${EXIT_OK} "platform that begins with a tilde" "${BUNDLE}"
	${OPT}output=out/platform tilde.bndlspec)
set(ENV{HOME} "${home}")
poco_osp_expect("no file is written in the home directory" NOT EXISTS "${WORK_DIR}/home/escaped")
list_bundle("${WORK_DIR}/out/platform/com.example.tilde_1.0.0.bndl" listing)
poco_osp_expect("the code is in bin/~/escaped" listing MATCHES "bin/~/escaped/libx.so")
# An empty --osname or --osarch names no directory.
write_code_spec(emptyname "")
poco_osp_expect_exit(FAILURE "empty --osname" "${BUNDLE}"
	${OPT}output=out/platform ${OPT}osname= ${OPT}osarch=arch emptyname.bndlspec)
poco_osp_expect_exit(FAILURE "empty --osarch" "${BUNDLE}"
	${OPT}output=out/platform ${OPT}osname=os ${OPT}osarch= emptyname.bndlspec)
poco_osp_expect("no bundle with an empty platform name is written"
	NOT EXISTS "${WORK_DIR}/out/platform/com.example.emptyname_1.0.0.bndl")
write_spec(codeoption "com.example.codeoption" "1.0.0" "")
poco_osp_expect_exit(FAILURE "empty --osarch with --code" "${BUNDLE}"
	${OPT}output=out/platform ${OPT}osname=os ${OPT}osarch= ${OPT}code=code/libx.so codeoption.bndlspec)
poco_osp_expect("no bundle with --code and an empty platform name is written"
	NOT EXISTS "${WORK_DIR}/out/platform/com.example.codeoption_1.0.0.bndl")

# The manifest has every field of the bundle specification; --bundle-dir-only writes only the bundle directory.
file(WRITE "${WORK_DIR}/full.bndlspec"
	"<?xml version=\"1.0\"?>\n"
	"<bundlespec>\n"
	"  <manifest>\n"
	"    <name>Full Bundle</name>\n"
	"    <symbolicName>com.example.full</symbolicName>\n"
	"    <version>1.2.3</version>\n"
	"    <vendor>Test</vendor>\n"
	"    <copyright>(c) Test</copyright>\n"
	"    <activator><class>FullActivator</class><library>Full</library></activator>\n"
	"    <lazyStart>true</lazyStart>\n"
	"    <sealed>true</sealed>\n"
	"    <preventUninstall>true</preventUninstall>\n"
	"    <runLevel>600</runLevel>\n"
	"    <extends>com.example.base</extends>\n"
	"    <dependency><symbolicName>com.example.a</symbolicName><version>[1.0,2.0)</version></dependency>\n"
	"    <dependency><symbolicName>com.example.b</symbolicName></dependency>\n"
	"    <module-dependency><symbolicName>com.example.module.a</symbolicName>"
	"<version>[1.0,2.0)</version></module-dependency>\n"
	"    <module><symbolicName>com.example.module.b</symbolicName><version>1.0.0</version></module>\n"
	"  </manifest>\n"
	"</bundlespec>\n")
poco_osp_expect_exit(${EXIT_OK} "build the bundle directory only" "${BUNDLE}"
	${OPT}output=out/full ${OPT}bundle-dir-only full.bndlspec)
file(GLOB outputFiles LIST_DIRECTORIES true "${WORK_DIR}/out/full/*")
poco_osp_expect("the output directory contains only the bundle directory"
	outputFiles STREQUAL "${WORK_DIR}/out/full/com.example.full_1.2.3")
file(READ "${WORK_DIR}/out/full/com.example.full_1.2.3/META-INF/manifest.mf" manifest)
string(CONCAT expectedManifest
	"Manifest-Version: 1.0\n"
	"Bundle-Name: Full Bundle\n"
	"Bundle-SymbolicName: com.example.full\n"
	"Bundle-Version: 1.2.3\n"
	"Bundle-Vendor: Test\n"
	"Bundle-Copyright: (c) Test\n"
	"Bundle-Activator: FullActivator;library=Full\n"
	"Bundle-RunLevel: 600\n"
	"Extends-Bundle: com.example.base\n"
	"Bundle-LazyStart: true\n"
	"Bundle-Sealed: true\n"
	"Bundle-PreventUninstall: true\n"
	"Require-Bundle: com.example.a;bundle-version=[1.0,2.0), \\\n"
	"                com.example.b\n"
	"Require-Module: com.example.module.a;module-version=[1.0,2.0)\n"
	"Provide-Module: com.example.module.b;module-version=1.0.0\n")
poco_osp_expect("the manifest has every field of the bundle specification" manifest STREQUAL expectedManifest)

# A bundle specification whose manifest cannot be read back gives no bundle.
file(WRITE "${WORK_DIR}/activator.bndlspec"
	"<?xml version=\"1.0\"?>\n"
	"<bundlespec>\n"
	"  <manifest>\n"
	"    <name>Test Bundle</name>\n"
	"    <symbolicName>com.example.activator</symbolicName>\n"
	"    <version>1.0.0</version>\n"
	"    <vendor>Test</vendor>\n"
	"    <activator><class>Activator</class><library>a/b</library></activator>\n"
	"  </manifest>\n"
	"</bundlespec>\n")
poco_osp_expect_exit(FAILURE "an activator library that is not a file name" "${BUNDLE}"
	${OPT}output=out/activator activator.bndlspec)
poco_osp_expect("no bundle is written for an activator library that is not a file name"
	NOT EXISTS "${WORK_DIR}/out/activator/com.example.activator_1.0.0.bndl")

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
file(GLOB outputFiles LIST_DIRECTORIES true "${WORK_DIR}/out/duplicate/*")
poco_osp_expect("a failed build leaves the output directory empty" NOT outputFiles)
poco_osp_expect_exit(FAILURE "two files with the same name, with --keep-bundle-dir" "${BUNDLE}"
	${OPT}output=out/duplicate-keep ${OPT}keep-bundle-dir duplicate.bndlspec)
file(GLOB outputFiles LIST_DIRECTORIES true "${WORK_DIR}/out/duplicate-keep/*")
poco_osp_expect("a failed build with --keep-bundle-dir leaves the output directory empty" NOT outputFiles)
file(MAKE_DIRECTORY "${WORK_DIR}/res/manifest/META-INF")
file(WRITE "${WORK_DIR}/res/manifest/META-INF/manifest.mf"
	"Manifest-Version: 1.0\nBundle-Name: Other\nBundle-SymbolicName: com.example.other\nBundle-Version: 1.0.0\n")
write_spec(manifest "com.example.manifest" "1.0.0" "res/manifest/*")
poco_osp_expect_exit(FAILURE "a file that replaces the manifest with that of another bundle" "${BUNDLE}"
	${OPT}output=out/manifest manifest.bndlspec)
write_spec(otherversion "com.example.other" "2.0.0" "res/manifest/*")
poco_osp_expect_exit(FAILURE "a file that replaces the manifest with that of another version" "${BUNDLE}"
	${OPT}output=out/manifest otherversion.bndlspec)
file(MAKE_DIRECTORY "${WORK_DIR}/res/samemanifest/META-INF")
file(WRITE "${WORK_DIR}/res/samemanifest/META-INF/manifest.mf"
	"Manifest-Version: 1.0\nBundle-Name: \${name}\nBundle-SymbolicName: com.example.same\nBundle-Version: 1.0.0\n")
write_spec(same "com.example.same" "1.0.0" "res/samemanifest/*")
poco_osp_expect_exit(${EXIT_OK} "a file that replaces the manifest with one of the same bundle" "${BUNDLE}"
	${OPT}output=out/manifest same.bndlspec)
file(MAKE_DIRECTORY "${WORK_DIR}/same")
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xf "${WORK_DIR}/out/manifest/com.example.same_1.0.0.bndl"
	WORKING_DIRECTORY "${WORK_DIR}/same"
	COMMAND_ERROR_IS_FATAL ANY)
file(READ "${WORK_DIR}/same/META-INF/manifest.mf" manifest)
poco_osp_expect("the bundle has the manifest from <files>" manifest MATCHES "Bundle-Name: \\\${name}")
# On a file system that ignores case, a file whose path differs in case from META-INF/manifest.mf replaces it too.
file(MAKE_DIRECTORY "${WORK_DIR}/res/casemanifest/meta-inf")
file(WRITE "${WORK_DIR}/res/casemanifest/meta-inf/MANIFEST.MF"
	"Manifest-Version: 1.0\nBundle-Name: Other\nBundle-SymbolicName: com.example.other\nBundle-Version: 1.0.0\n")
write_spec(casemanifest "com.example.casemanifest" "1.0.0" "res/casemanifest/*")
if(EXISTS "${WORK_DIR}/res/casemanifest/META-INF/manifest.mf")
	set(expected FAILURE)
else()
	set(expected ${EXIT_OK})
endif()
poco_osp_expect_exit(${expected} "a file whose path differs in case from that of the manifest" "${BUNDLE}"
	${OPT}output=out/casemanifest casemanifest.bndlspec)
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
	# The extra field length is at offset 28 of the local file header.
	file(READ "${bundle}" header LIMIT 30 HEX)
	string(SUBSTRING "${header}" 56 4 extraFieldLength)
	poco_osp_expect("the first entry has no extra field" extraFieldLength STREQUAL "0000")

	# The bundle is assembled and written under temporary names, which a bundle repository does not load.
	# A zip command that records its working directory and output file stands in for the real one.
	file(MAKE_DIRECTORY "${WORK_DIR}/fakezip")
	file(WRITE "${WORK_DIR}/fakezip/zip"
		"#!/bin/sh\n"
		"pwd > \"${WORK_DIR}/zip-directory.txt\"\n"
		"while [ $# -gt 2 ]; do shift; done\n"
		"echo \"$1\" > \"${WORK_DIR}/zip-output.txt\"\n"
		": > \"$1\"\n")
	file(CHMOD "${WORK_DIR}/fakezip/zip" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
	write_spec(temporary "com.example.temporary" "1.0.0" "res/links")
	poco_osp_expect_exit(${EXIT_OK} "build with a zip command that records its arguments" "${CMAKE_COMMAND}" -E env
		"PATH=${WORK_DIR}/fakezip:$ENV{PATH}" "${BUNDLE}" -o out/temporary --symlinks temporary.bndlspec)
	file(STRINGS "${WORK_DIR}/zip-directory.txt" zipDirectory)
	poco_osp_expect("the bundle directory has a temporary name while it is written"
		zipDirectory MATCHES "/tmp[^/]*\\.tmp$")
	file(STRINGS "${WORK_DIR}/zip-output.txt" zipOutput)
	poco_osp_expect("the bundle file has a temporary name while it is written"
		zipOutput MATCHES "/tmp[^/]*\\.tmp$")
	file(GLOB outputFiles LIST_DIRECTORIES true "${WORK_DIR}/out/temporary/*")
	poco_osp_expect("the output directory contains only the bundle file"
		outputFiles STREQUAL "${WORK_DIR}/out/temporary/com.example.temporary_1.0.0.bndl")

	# --strip runs the strip command once on each copied library, but not on the target of a copied link.
	file(MAKE_DIRECTORY "${WORK_DIR}/strip/code" "${WORK_DIR}/strip/lib")
	file(WRITE "${WORK_DIR}/strip/lib/libreal.so" "library\n")
	file(CREATE_LINK "${WORK_DIR}/strip/lib/libreal.so" "${WORK_DIR}/strip/code/libx.so" SYMBOLIC)
	file(WRITE "${WORK_DIR}/strip/code/liby.so" "library\n")
	file(WRITE "${WORK_DIR}/strip/fakestrip.sh" "#!/bin/sh\necho stripped >> \"$1\"\n")
	file(CHMOD "${WORK_DIR}/strip/fakestrip.sh" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
	write_spec(strip "com.example.strip" "1.0.0" "")
	poco_osp_expect_exit(${EXIT_OK} "build with --symlinks and --strip" "${BUNDLE}"
		-o out/strip --symlinks --keep-bundle-dir --strip=${WORK_DIR}/strip/fakestrip.sh
		--osname=os --osarch=arch --code=strip/code/libx.so,strip/code/liby.so strip.bndlspec)
	file(READ "${WORK_DIR}/strip/lib/libreal.so" content)
	poco_osp_expect("the target of a copied link is not stripped" content STREQUAL "library\n")
	file(READ "${WORK_DIR}/out/strip/com.example.strip_1.0.0/bin/os/arch/liby.so" content)
	poco_osp_expect("the copied library is stripped once" content STREQUAL "library\nstripped\n")
	file(READ "${WORK_DIR}/strip/code/liby.so" content)
	poco_osp_expect("the original library is not stripped" content STREQUAL "library\n")
	# Each <code> element strips only the files that it copies.
	file(MAKE_DIRECTORY "${WORK_DIR}/strip/two")
	file(WRITE "${WORK_DIR}/strip/two/liba.so" "library\n")
	file(WRITE "${WORK_DIR}/strip/two/libb.so" "library\n")
	file(WRITE "${WORK_DIR}/twocode.bndlspec"
		"<?xml version=\"1.0\"?>\n"
		"<bundlespec>\n"
		"  <manifest>\n"
		"    <name>Test Bundle</name>\n"
		"    <symbolicName>com.example.twocode</symbolicName>\n"
		"    <version>1.0.0</version>\n"
		"    <vendor>Test</vendor>\n"
		"  </manifest>\n"
		"  <code platform=\"os/arch\">strip/two/liba.so</code>\n"
		"  <code platform=\"os/arch\">strip/two/libb.so</code>\n"
		"</bundlespec>\n")
	poco_osp_expect_exit(${EXIT_OK} "build with --strip and two <code> elements for one platform" "${BUNDLE}"
		-o out/twocode --keep-bundle-dir --strip=${WORK_DIR}/strip/fakestrip.sh twocode.bndlspec)
	file(READ "${WORK_DIR}/out/twocode/com.example.twocode_1.0.0/bin/os/arch/liba.so" content)
	poco_osp_expect("the library of the first <code> element is stripped once" content STREQUAL "library\nstripped\n")
endif()

file(REMOVE_RECURSE "${WORK_DIR}")
