#
# BundleGeneratedTest.cmake
#
# Tests of the GENERATED directories of poco_osp_bundle() in PocoOspBundle.cmake
# with a copy of the project in BundleGenerated and the bundle creator of the
# build: a file below bundle/ is a dependency of the bundle unless it is in a
# directory that GENERATED names, a build after the files of such a directory
# changed does not configure again, a writer of such a directory whose stamp is
# in DEPENDS creates the bundle again, a custom target in DEPENDS that is
# defined before the call, without a file, gives a warning, a name is compared
# as a normal path, and a name that is not a directory below bundle/, relative
# to it, stops the configure.
#
# Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
# and Contributors.
#
# SPDX-License-Identifier:	BSL-1.0
#
# Variables: BUNDLE_CREATOR (the bundle creator), GENERATOR (the CMake
# generator), WORK_DIR (a directory for this test only, removed afterwards).
#

include("${CMAKE_CURRENT_LIST_DIR}/../PocoOSPScriptTest.cmake")

file(REMOVE_RECURSE "${WORK_DIR}")
# A copy of the project, whose bundle/ directories the test changes.
set(SOURCE "${WORK_DIR}/source")
file(COPY "${CMAKE_CURRENT_LIST_DIR}/BundleGenerated/" DESTINATION "${SOURCE}")
set(BUILD "${WORK_DIR}/build")
set(BUNDLES plain generated dot other nested written)
# [id] is a glob expression that matches d. Make reads it as a wildcard in a prerequisite, so a bundle of a
# Makefiles build cannot depend on the files of such a directory.
if(GENERATOR MATCHES "Makefiles")
	message(WARNING "The checks of a directory whose name is a glob expression are skipped with ${GENERATOR}.")
	set(ids "d/d.txt=d 1")
else()
	file(WRITE "${SOURCE}/bundle/[id]/id.txt" "id 1\n")
	set(ids "[id]/id.txt=id 1 d/d.txt=d 1")
endif()

# Builds the project and stores the content of each bundle in the variable named after its directory: the files
# other than the manifest as <path>=<content>, separated by spaces.
function(build description)
	poco_osp_expect_exit(${EXIT_OK} "${description}" "${CMAKE_COMMAND}" --build "${BUILD}")
	foreach(bundle IN LISTS BUNDLES)
		set(dir "${WORK_DIR}/content/${bundle}")
		file(REMOVE_RECURSE "${dir}")
		file(GLOB bundle_file "${BUILD}/${bundle}/*.bndl")
		file(ARCHIVE_EXTRACT INPUT "${bundle_file}" DESTINATION "${dir}")
		file(GLOB_RECURSE files RELATIVE "${dir}" "${dir}/*")
		list(FILTER files EXCLUDE REGEX "^META-INF/")
		set(content "")
		foreach(file IN LISTS files)
			file(READ "${dir}/${file}" text)
			string(STRIP "${text}" text)
			list(APPEND content "${file}=${text}")
		endforeach()
		list(JOIN content " " content)
		set(${bundle} "${content}" PARENT_SCOPE)
	endforeach()
endfunction()

poco_osp_expect_exit(${EXIT_OK} "configure" "${CMAKE_COMMAND}" -S "${SOURCE}" -B "${BUILD}" -G "${GENERATOR}"
	-DMODULE_DIR=${CMAKE_CURRENT_LIST_DIR}/.. -DBUNDLE_CREATOR=${BUNDLE_CREATOR})
build("build")
foreach(bundle IN ITEMS plain generated dot other)
	poco_osp_expect("the bundle in ${bundle} contains the files of bundle/ (${${bundle}})"
		${bundle} STREQUAL "${ids} data.txt=data 1 generated/page.html=page 1")
endforeach()
poco_osp_expect("the bundle in nested contains the files of bundle/ (${nested})"
	nested STREQUAL "generated/dist/app.js=app 1 generated/index.html=index 1")
poco_osp_expect("the bundle in written contains the files of its writer (${written})"
	written STREQUAL "app/app-1.js=app 1 data.txt=data 1")

poco_osp_wait_for_newer_time()
file(WRITE "${SOURCE}/bundle/generated/page.html" "page 2\n")
build("build after a change in the directory generated")
poco_osp_expect("without GENERATED a changed file below bundle/ creates the bundle again (${plain})"
	plain STREQUAL "${ids} data.txt=data 1 generated/page.html=page 2")
poco_osp_expect("a changed file in a GENERATED directory does not create the bundle again (${generated})"
	generated STREQUAL "${ids} data.txt=data 1 generated/page.html=page 1")
poco_osp_expect("a changed file in a GENERATED directory named with ./ does not create the bundle again (${dot})"
	dot STREQUAL "${ids} data.txt=data 1 generated/page.html=page 1")
poco_osp_expect("with GENERATED naming another directory a changed file creates the bundle again (${other})"
	other STREQUAL "${ids} data.txt=data 1 generated/page.html=page 2")

poco_osp_wait_for_newer_time()
file(WRITE "${SOURCE}/bundle/data.txt" "data 2\n")
build("build after a change outside the directory generated")
foreach(bundle IN ITEMS plain generated dot other)
	poco_osp_expect("a changed file outside the GENERATED directories creates the bundle in ${bundle} again (${${bundle}})"
		${bundle} STREQUAL "${ids} data.txt=data 2 generated/page.html=page 2")
endforeach()

if(NOT GENERATOR MATCHES "Makefiles")
	poco_osp_wait_for_newer_time()
	file(WRITE "${SOURCE}/bundle/[id]/id.txt" "id 2\n")
	build("build after a change in a directory whose name is a glob expression")
	set(ids "[id]/id.txt=id 2 d/d.txt=d 1")
	poco_osp_expect("a changed file in a directory whose name is a glob expression creates the bundle again (${plain})"
		plain STREQUAL "${ids} data.txt=data 2 generated/page.html=page 2")
endif()

poco_osp_wait_for_newer_time()
file(REMOVE "${SOURCE}/bundle/generated/page.html")
file(WRITE "${SOURCE}/bundle/generated/page-3.html" "page 3\n")
build("build after the files of the directory generated were replaced by files of other names")
poco_osp_expect("without GENERATED the bundle follows the files of other names (${plain})"
	plain STREQUAL "${ids} data.txt=data 2 generated/page-3.html=page 3")
poco_osp_expect("with GENERATED naming another directory the bundle follows the files of other names (${other})"
	other STREQUAL "${ids} data.txt=data 2 generated/page-3.html=page 3")
poco_osp_expect("files of other names in a GENERATED directory do not create the bundle again (${generated})"
	generated STREQUAL "${ids} data.txt=data 2 generated/page.html=page 2")
poco_osp_expect("files of other names in a GENERATED directory named with ./ do not create the bundle again (${dot})"
	dot STREQUAL "${ids} data.txt=data 2 generated/page.html=page 2")

poco_osp_wait_for_newer_time()
file(WRITE "${SOURCE}/nested/bundle/generated/dist/app.js" "app 2\n")
build("build after a change in a nested GENERATED directory")
poco_osp_expect("a changed file in a nested GENERATED directory does not create the bundle again (${nested})"
	nested STREQUAL "generated/dist/app.js=app 1 generated/index.html=index 1")

poco_osp_wait_for_newer_time()
file(WRITE "${SOURCE}/nested/bundle/generated/index.html" "index 2\n")
build("build after a change next to a nested GENERATED directory")
poco_osp_expect("a changed file next to a nested GENERATED directory creates the bundle again (${nested})"
	nested STREQUAL "generated/dist/app.js=app 2 generated/index.html=index 2")

poco_osp_wait_for_newer_time()
file(SIZE "${BUILD}/configure-count" before)
file(REMOVE "${SOURCE}/nested/bundle/generated/dist/app.js")
file(WRITE "${SOURCE}/nested/bundle/generated/dist/app-3.js" "app 3\n")
build("build after the files of a GENERATED directory were replaced by files of other names")
file(SIZE "${BUILD}/configure-count" after)
poco_osp_expect("files of other names in a GENERATED directory do not configure again (${before}, then ${after})"
	after EQUAL before)
poco_osp_expect("files of other names in a nested GENERATED directory do not create the bundle again (${nested})"
	nested STREQUAL "generated/dist/app.js=app 2 generated/index.html=index 2")

poco_osp_wait_for_newer_time()
file(SIZE "${BUILD}/configure-count" before)
file(WRITE "${SOURCE}/app-version.txt" "2\n")
build("build after the writer of a GENERATED directory replaced its files by files of other names")
file(SIZE "${BUILD}/configure-count" after)
poco_osp_expect("a writer whose stamp is in DEPENDS does not configure again (${before}, then ${after})"
	after EQUAL before)
poco_osp_expect("a writer whose stamp is in DEPENDS creates the bundle again with its new files (${written})"
	written STREQUAL "app/app-2.js=app 2 data.txt=data 1")

execute_process(COMMAND "${CMAKE_COMMAND}" -S "${SOURCE}" -B "${WORK_DIR}/custom-target" -G "${GENERATOR}"
		-DMODULE_DIR=${CMAKE_CURRENT_LIST_DIR}/.. -DBUNDLE_CREATOR=${BUNDLE_CREATOR} -DCUSTOM_TARGET_WRITER=ON
	RESULT_VARIABLE result
	OUTPUT_VARIABLE output
	ERROR_VARIABLE output)
poco_osp_expect("a custom target in DEPENDS without a file gives a warning with GENERATED only:\n${output}"
	result EQUAL 0 AND output MATCHES "OSP bundle BundleTargetOnly: DEPENDS names the custom target"
	AND NOT output MATCHES "OSP bundle BundleTargetAndStamp:"
	AND NOT output MATCHES "OSP bundle BundleTargetWithoutGenerated:")
# The check runs during the call, when a custom target defined later does not exist yet and its name is taken as a file.
poco_osp_expect("a custom target in DEPENDS that is defined after the call gives no warning:\n${output}"
	result EQUAL 0 AND NOT output MATCHES "OSP bundle BundleTargetLater:")

# Configures the project with GENERATED_INVALID set to the given name in its own build directory and checks that the
# configure stops.
function(configure_invalid description build name)
	execute_process(COMMAND "${CMAKE_COMMAND}" -S "${SOURCE}" -B "${WORK_DIR}/${build}" -G "${GENERATOR}"
			-DMODULE_DIR=${CMAKE_CURRENT_LIST_DIR}/.. -DBUNDLE_CREATOR=${BUNDLE_CREATOR} -DGENERATED_INVALID=${name}
		RESULT_VARIABLE result
		OUTPUT_VARIABLE output
		ERROR_VARIABLE output)
	poco_osp_expect("${description} stops the configure:\n${output}"
		NOT result EQUAL 0 AND output MATCHES "OSP bundle BundleInvalid: GENERATED")
endfunction()

configure_invalid("an absolute path in GENERATED" invalid-absolute "${SOURCE}/bundle/generated")
configure_invalid("a path in GENERATED that starts with bundle/" invalid-bundle bundle/generated)
configure_invalid("a path in GENERATED that starts with .." invalid-parent ../bundle/generated)
configure_invalid("a path in GENERATED that leads out of bundle/ after normalisation" invalid-normal generated/../..)
configure_invalid("the root directory in GENERATED" invalid-root /)
configure_invalid("bundle/ in GENERATED" invalid-bundle-dir bundle/)
configure_invalid("a path in GENERATED that names bundle/ itself" invalid-self .)
configure_invalid("a path in GENERATED that names bundle/ itself after normalisation" invalid-self-normal generated/..)

file(REMOVE_RECURSE "${WORK_DIR}")
