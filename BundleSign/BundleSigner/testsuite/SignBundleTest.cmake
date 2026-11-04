#
# SignBundleTest.cmake
#
# Tests of the signbundle tool: signing, and verification that accepts
# only bundles signed by a trusted signer.
#
# Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
# and Contributors.
#
# SPDX-License-Identifier:	BSL-1.0
#
# Variables: SIGNBUNDLE (the tool), DATA_DIR (test certificates and keys),
# WORK_DIR (a directory for this test only, removed afterwards).
#

include("${CMAKE_CURRENT_LIST_DIR}/../../../cmake/PocoOSPScriptTest.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../../testsuite/SymlinkBundleCheck.cmake")

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}/content/META-INF")
file(WRITE "${WORK_DIR}/content/META-INF/manifest.mf" "Manifest-Version: 1.0\n")
file(WRITE "${WORK_DIR}/content/bundle.properties" "foo: bar\n")
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar cf "${WORK_DIR}/unsigned.bndl" --format=zip META-INF bundle.properties
	WORKING_DIRECTORY "${WORK_DIR}/content"
	COMMAND_ERROR_IS_FATAL ANY)

file(COPY_FILE "${WORK_DIR}/unsigned.bndl" "${WORK_DIR}/signed.bndl")
poco_osp_expect_exit(${EXIT_OK} "sign" "${SIGNBUNDLE}"
	${OPT}key=${DATA_DIR}/signer.pem ${OPT}certificate=${DATA_DIR}/signer.pem ${OPT}alias=test.signer signed.bndl)
poco_osp_expect_exit(${EXIT_OK} "verify with the signer certificate" "${SIGNBUNDLE}"
	${OPT}verify ${OPT}alias=test.signer ${OPT}certificate=${DATA_DIR}/signer.pem signed.bndl)
poco_osp_expect_exit(${EXIT_OK} "verify the first signer" "${SIGNBUNDLE}"
	${OPT}verify ${OPT}certificate=${DATA_DIR}/signer.pem signed.bndl)
poco_osp_expect_exit(${EXIT_USAGE} "verify without a trusted certificate" "${SIGNBUNDLE}"
	${OPT}verify ${OPT}alias=test.signer signed.bndl)
poco_osp_expect_exit(${EXIT_DATAERR} "verify an unsigned bundle" "${SIGNBUNDLE}"
	${OPT}verify ${OPT}alias=test.signer ${OPT}certificate=${DATA_DIR}/signer.pem unsigned.bndl)

file(COPY_FILE "${WORK_DIR}/unsigned.bndl" "${WORK_DIR}/passphrase.bndl")
file(WRITE "${WORK_DIR}/passphrase.txt" "test-passphrase\n")
poco_osp_expect_exit(${EXIT_OK} "sign with an encrypted key and a passphrase file" "${SIGNBUNDLE}"
	${OPT}key=${DATA_DIR}/signer-encrypted.pem ${OPT}passphrase-file=passphrase.txt ${OPT}alias=test.signer
	passphrase.bndl)
poco_osp_expect_exit(${EXIT_OK} "verify the bundle signed with the encrypted key" "${SIGNBUNDLE}"
	${OPT}verify ${OPT}alias=test.signer ${OPT}certificate=${DATA_DIR}/signer.pem passphrase.bndl)
file(COPY_FILE "${WORK_DIR}/unsigned.bndl" "${WORK_DIR}/passphrase1.bndl")
file(COPY_FILE "${WORK_DIR}/unsigned.bndl" "${WORK_DIR}/passphrase2.bndl")
poco_osp_expect_exit(${EXIT_OK} "sign two bundles with an encrypted key" "${SIGNBUNDLE}"
	${OPT}key=${DATA_DIR}/signer-encrypted.pem ${OPT}passphrase-file=passphrase.txt ${OPT}alias=test.signer
	passphrase1.bndl passphrase2.bndl)
poco_osp_expect_exit(${EXIT_OK} "verify the second bundle signed with the encrypted key" "${SIGNBUNDLE}"
	${OPT}verify ${OPT}alias=test.signer ${OPT}certificate=${DATA_DIR}/signer.pem passphrase2.bndl)

# A system policy can forbid SHA1 signatures; any other failure of the probe fails the signing step below.
file(COPY_FILE "${WORK_DIR}/unsigned.bndl" "${WORK_DIR}/sha1-probe.bndl")
execute_process(COMMAND "${SIGNBUNDLE}" ${OPT}key=${DATA_DIR}/signer.pem ${OPT}digest-type=SHA1 ${OPT}alias=test.signer
	sha1-probe.bndl
	WORKING_DIRECTORY "${WORK_DIR}"
	RESULT_VARIABLE result
	OUTPUT_VARIABLE output
	ERROR_VARIABLE output)
if(NOT result EQUAL 0 AND output MATCHES "EVP_PKEY_CTX_set_signature_md\\(\\): [^\n]*invalid digest")
	message(WARNING "OpenSSL refuses SHA1 signatures, SHA1 steps skipped:\n${output}")
else()
	file(COPY_FILE "${WORK_DIR}/unsigned.bndl" "${WORK_DIR}/sha1.bndl")
	poco_osp_expect_exit(${EXIT_OK} "sign with SHA1" "${SIGNBUNDLE}"
		${OPT}key=${DATA_DIR}/signer.pem ${OPT}digest-type=SHA1 ${OPT}alias=test.signer sha1.bndl)
	poco_osp_expect_exit(${EXIT_DATAERR} "verify a SHA1 signature" "${SIGNBUNDLE}"
		${OPT}verify ${OPT}alias=test.signer ${OPT}certificate=${DATA_DIR}/signer.pem sha1.bndl)
	poco_osp_expect_exit(${EXIT_OK} "verify a SHA1 signature with --allow-sha1" "${SIGNBUNDLE}"
		${OPT}verify ${OPT}allow-sha1 ${OPT}alias=test.signer ${OPT}certificate=${DATA_DIR}/signer.pem sha1.bndl)
endif()

file(MAKE_DIRECTORY "${WORK_DIR}/stripped")
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xf "${WORK_DIR}/signed.bndl"
	WORKING_DIRECTORY "${WORK_DIR}/stripped"
	COMMAND_ERROR_IS_FATAL ANY)
file(READ "${WORK_DIR}/stripped/META-INF/test.signer.rsa" signatureBlock)
if(NOT signatureBlock MATCHES "RSA-SHA256-Signature: ")
	message(SEND_ERROR "signbundle does not sign with SHA256 by default:\n${signatureBlock}")
endif()
file(REMOVE "${WORK_DIR}/stripped/META-INF/test.signer.sf" "${WORK_DIR}/stripped/META-INF/test.signer.rsa")
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar cf "${WORK_DIR}/stripped.bndl" --format=zip META-INF bundle.properties
	WORKING_DIRECTORY "${WORK_DIR}/stripped"
	COMMAND_ERROR_IS_FATAL ANY)
poco_osp_expect_exit(${EXIT_DATAERR} "verify a bundle without its signature files" "${SIGNBUNDLE}"
	${OPT}verify ${OPT}alias=test.signer ${OPT}certificate=${DATA_DIR}/signer.pem stripped.bndl)

file(COPY_FILE "${WORK_DIR}/signed.bndl" "${WORK_DIR}/resigned.bndl")
poco_osp_expect_exit(${EXIT_OK} "re-sign with another key" "${SIGNBUNDLE}"
	${OPT}key=${DATA_DIR}/foreign.pem ${OPT}certificate=${DATA_DIR}/foreign.pem ${OPT}alias=test.signer resigned.bndl)
poco_osp_expect_exit(${EXIT_DATAERR} "verify a bundle re-signed with another key" "${SIGNBUNDLE}"
	${OPT}verify ${OPT}alias=test.signer ${OPT}certificate=${DATA_DIR}/signer.pem resigned.bndl)

file(COPY_FILE "${WORK_DIR}/unsigned.bndl" "${WORK_DIR}/issued.bndl")
poco_osp_expect_exit(${EXIT_OK} "sign with a certificate issued by the test CA" "${SIGNBUNDLE}"
	${OPT}key=${DATA_DIR}/issued.pem ${OPT}certificate=${DATA_DIR}/issued.pem ${OPT}alias=test.signer issued.bndl)
poco_osp_expect_exit(${EXIT_OK} "verify with the CA file" "${SIGNBUNDLE}"
	${OPT}verify ${OPT}alias=test.signer ${OPT}ca-file=${DATA_DIR}/ca.pem issued.bndl)
poco_osp_expect_exit(${EXIT_DATAERR} "verify with an unrelated CA file" "${SIGNBUNDLE}"
	${OPT}verify ${OPT}alias=test.signer ${OPT}ca-file=${DATA_DIR}/ca.pem signed.bndl)

# The signer alias is the name of the signature files in META-INF.
file(COPY_FILE "${WORK_DIR}/unsigned.bndl" "${WORK_DIR}/alias.bndl")
poco_osp_expect_exit(FAILURE "sign with an alias that is not a file name" "${SIGNBUNDLE}"
	${OPT}key=${DATA_DIR}/signer.pem ${OPT}alias=team/signer alias.bndl)

# A bundle written by Info-ZIP zip, whose entries have extra fields, stays intact when it is signed.
file(COPY_FILE "${DATA_DIR}/extra-field.bndl" "${WORK_DIR}/extra-field.bndl")
poco_osp_expect_exit(${EXIT_OK} "sign a bundle whose entries have extra fields" "${SIGNBUNDLE}"
	${OPT}key=${DATA_DIR}/signer.pem ${OPT}certificate=${DATA_DIR}/signer.pem ${OPT}alias=test.signer extra-field.bndl)
poco_osp_expect_exit(${EXIT_OK} "verify the bundle whose entries have extra fields" "${SIGNBUNDLE}"
	${OPT}verify ${OPT}alias=test.signer ${OPT}certificate=${DATA_DIR}/signer.pem extra-field.bndl)

# The same for a bundle whose directory entries have extra fields shorter than a ZIP64 extra field.
file(COPY_FILE "${DATA_DIR}/short-extra-field.bndl" "${WORK_DIR}/short-extra-field.bndl")
poco_osp_expect_exit(${EXIT_OK} "sign a bundle whose entries have short extra fields" "${SIGNBUNDLE}"
	${OPT}key=${DATA_DIR}/signer.pem ${OPT}certificate=${DATA_DIR}/signer.pem ${OPT}alias=test.signer
	short-extra-field.bndl)
poco_osp_expect_exit(${EXIT_OK} "verify the bundle whose entries have short extra fields" "${SIGNBUNDLE}"
	${OPT}verify ${OPT}alias=test.signer ${OPT}certificate=${DATA_DIR}/signer.pem short-extra-field.bndl)

# A bundle written with --symlinks keeps its symbolic link and the mode of its executable file when it is signed.
if(NOT WIN32)
	file(COPY_FILE "${DATA_DIR}/symlinks.bndl" "${WORK_DIR}/symlinks.bndl")
	poco_osp_expect_exit(${EXIT_OK} "sign a bundle with a symbolic link" "${SIGNBUNDLE}"
		${OPT}key=${DATA_DIR}/signer.pem ${OPT}certificate=${DATA_DIR}/signer.pem ${OPT}alias=test.signer symlinks.bndl)
	poco_osp_expect_exit(${EXIT_OK} "verify the bundle with a symbolic link" "${SIGNBUNDLE}"
		${OPT}verify ${OPT}alias=test.signer ${OPT}certificate=${DATA_DIR}/signer.pem symlinks.bndl)
	expect_link_and_mode_kept("the signed bundle" "${WORK_DIR}/symlinks.bndl" "${WORK_DIR}/symlinks")
endif()

# A bundle whose links or executables would lose their attributes, or whose attributes are unknown, is left as it is.
foreach(bundle symlink-extra-field.bndl symlink-fat-host-extra-field.bndl executable-extra-field.bndl
	central-name-mismatch.bndl)
	file(COPY_FILE "${DATA_DIR}/${bundle}" "${WORK_DIR}/${bundle}")
	poco_osp_expect_exit(${EXIT_DATAERR} "sign ${bundle}" "${SIGNBUNDLE}"
		${OPT}key=${DATA_DIR}/signer.pem ${OPT}certificate=${DATA_DIR}/signer.pem ${OPT}alias=test.signer ${bundle})
	file(SHA256 "${DATA_DIR}/${bundle}" originalHash)
	file(SHA256 "${WORK_DIR}/${bundle}" hash)
	poco_osp_expect("${bundle} is unchanged after signing failed" hash STREQUAL originalHash)
endforeach()

# A malformed bundle fails verification like any other bundle that does not verify.
poco_osp_expect_exit(${EXIT_DATAERR} "verify the first signer of a bundle with a duplicate entry" "${SIGNBUNDLE}"
	${OPT}verify ${OPT}certificate=${DATA_DIR}/signer.pem ${DATA_DIR}/duplicate.bndl)
poco_osp_expect_exit(${EXIT_DATAERR} "verify a bundle with a duplicate entry" "${SIGNBUNDLE}"
	${OPT}verify ${OPT}alias=test.signer ${OPT}certificate=${DATA_DIR}/signer.pem ${DATA_DIR}/duplicate.bndl)

# A trusted certificate without an RSA key is skipped.
file(COPY_FILE "${WORK_DIR}/unsigned.bndl" "${WORK_DIR}/nocert.bndl")
poco_osp_expect_exit(${EXIT_OK} "sign without a certificate in the bundle" "${SIGNBUNDLE}"
	${OPT}key=${DATA_DIR}/signer.pem ${OPT}alias=test.signer nocert.bndl)
set(caFile "")
foreach(pem ec.pem signer.pem)
	file(READ "${DATA_DIR}/${pem}" content)
	string(REGEX MATCH "-----BEGIN CERTIFICATE-----[^-]*-----END CERTIFICATE-----\n" certificate "${content}")
	string(APPEND caFile "${certificate}")
endforeach()
file(WRITE "${WORK_DIR}/ec-and-signer.pem" "${caFile}")
poco_osp_expect_exit(${EXIT_OK} "verify with a CA file whose first certificate has no RSA key" "${SIGNBUNDLE}"
	${OPT}verify ${OPT}alias=test.signer ${OPT}ca-file=ec-and-signer.pem nocert.bndl)

file(REMOVE_RECURSE "${WORK_DIR}")
