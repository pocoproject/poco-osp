# A project that copies its directory data into the build directory with _poco_osp_copy_directory(), as
# the build copies the test bundles and bundle resources, and adds extra.txt to the copy afterwards.

cmake_minimum_required(VERSION 3.26)
project(DirectoryCopyTest NONE)

include("${MODULE_DIR}/PocoOspBundle.cmake")

_poco_osp_copy_directory(Data ${CMAKE_CURRENT_SOURCE_DIR}/data ${CMAKE_CURRENT_BINARY_DIR}/data
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/extra.txt
	COMMANDS COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/extra.txt ${CMAKE_CURRENT_BINARY_DIR}/data
)
