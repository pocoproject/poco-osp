# A project that generates sources with poco_remote_gen(); a stand-in for RemoteGenNG
# (POCO_REMOTEGEN_NG) writes the directory generated, with CLIENT also the directory client, and
# counts its runs in runs.txt. GEN_DIR replaces the GEN_DIR_NAME generated of the first call, CONFIG
# its configuration RemoteGen.xml, and EXTRA_DEFINES adds properties to its DEFINES.

cmake_minimum_required(VERSION 3.26)
project(RemoteGenTest LANGUAGES CXX)

include("${MODULE_DIR}/PocoOspRemoteGen.cmake")

# With DEFAULT_OSP_BASE, poco_remote_gen() chooses POCO_OSP_BASE.
if(NOT DEFAULT_OSP_BASE)
	set(POCO_OSP_BASE "${CMAKE_CURRENT_SOURCE_DIR}/base")
endif()
# With SUBDIRECTORY, the directory SUBDIRECTORY calls poco_remote_gen() with GEN_DIR and CONFIG, and this
# directory only with TOP_CALL; SUBDIRECTORY_BINARY is its binary directory.
if(DEFINED SUBDIRECTORY)
	add_subdirectory(${SUBDIRECTORY} ${SUBDIRECTORY_BINARY})
	if(NOT TOP_CALL)
		return()
	endif()
endif()
if(NOT DEFINED GEN_DIR)
	set(GEN_DIR generated)
endif()
if(NOT DEFINED CONFIG)
	set(CONFIG RemoteGen.xml)
endif()
poco_remote_gen(${CONFIG} GEN_DIR_NAME "${GEN_DIR}" DEFINES ${EXTRA_DEFINES})
if(CLIENT)
	poco_remote_gen(RemoteGenClient.xml GEN_DIR_NAME client)
endif()
