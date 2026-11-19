# A project that creates the bundles com.example.a and com.example.a.b with poco_osp_bundle(), each
# installed with its own component, with MISSPELT_KEYWORD a call with a misspelt keyword, and with
# CODE_DEFINED_LATER a call whose CODE names a library that is defined after it.

cmake_minimum_required(VERSION 3.26)
project(BundleFunctionTest NONE)

include("${MODULE_DIR}/PocoOspBundle.cmake")

add_executable(Poco::OSP::BundleCreator IMPORTED)
set_target_properties(Poco::OSP::BundleCreator PROPERTIES IMPORTED_LOCATION "${BUNDLE_CREATOR}")

poco_osp_bundle(BundleA
	SPEC ${CMAKE_CURRENT_SOURCE_DIR}/a/a.bndlspec
	DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/bundles
	INSTALL_COMPONENT A
)
poco_osp_bundle(BundleAB
	SPEC ${CMAKE_CURRENT_SOURCE_DIR}/ab/ab.bndlspec
	DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/bundles
	INSTALL_COMPONENT AB
)
if(MISSPELT_KEYWORD)
	poco_osp_bundle(BundleTypo
		SPEC ${CMAKE_CURRENT_SOURCE_DIR}/a/a.bndlspec
		DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/typo
		DEPEND BundleA
	)
endif()
if(CODE_DEFINED_LATER)
	poco_osp_bundle(BundleLate
		SPEC ${CMAKE_CURRENT_SOURCE_DIR}/a/a.bndlspec
		DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/late
		CODE $<TARGET_FILE:Late>
	)
	enable_language(CXX)
	add_library(Late MODULE late.cpp)
endif()
