
option(ENABLE_REMOTING "Enable Remoting" OFF)
option(ENABLE_JS "Enable Javascript with NodeJS" OFF)
option(ENABLE_GEO "Enable Geo package" OFF)

option(ENABLE_SERIAL "Enable serial package" OFF)
option(ENABLE_WEBTUNNEL "Enable web tunnel" OFF)
option(ENABLE_XSDVALIDATOR "Enable XSD Validator (runtime XML-against-XSD validation, requires libxml2); superprojects enable it when their code depends on it" OFF)

option(ENABLE_OSP "Enable OSP" OFF)
option(ENABLE_OSP_BUNDLE_CREATOR "Enable OSP bundle creator" OFF)
option(ENABLE_OSP_BUNDLES "Enable creating OSP bundles" OFF)

option(ENABLE_OSP_AUTH "Enable OSP Auth package" OFF)
option(ENABLE_OSP_AUTH_LDAP "Enable OSP Auth with LDAP" OFF)

if(ENABLE_OSP_AUTH)
	set(ENABLE_OSP ON CACHE BOOL "Enable OSP" FORCE)
	set(ENABLE_UTIL ON CACHE BOOL "Enable Util" FORCE)
	set(ENABLE_DATA ON CACHE BOOL "Enable Data" FORCE)
endif()


if(ENABLE_REMOTING)
	set(ENABLE_NET ON CACHE BOOL "Enable Net" FORCE)
	set(ENABLE_UTIL ON CACHE BOOL "Enable Util" FORCE)
	set(ENABLE_CPPPARSER ON CACHE BOOL "Enable CPPParser" FORCE)
	set(ENABLE_XML ON CACHE BOOL "Enable XML" FORCE)
	set(ENABLE_JSON ON CACHE BOOL "Enable XML" FORCE)
endif()

if(ENABLE_OSP_BUNDLES)
	set(ENABLE_UTIL ON CACHE BOOL "Enable Util" FORCE)
	set(ENABLE_CPPPARSER ON CACHE BOOL "Enable CPPParser" FORCE)
	set(ENABLE_XML ON CACHE BOOL "Enable XML" FORCE)
	set(ENABLE_JSON ON CACHE BOOL "Enable XML" FORCE)

	set(ENABLE_OSP_BUNDLE_CREATOR ON CACHE BOOL "Enable OSP bundle creator" FORCE)
	list(APPEND Poco_COMPONENTS "OSP Bundles")
endif()

if(ENABLE_OSP_BUNDLE_CREATOR)
	set(ENABLE_CPPPARSER ON CACHE BOOL "Enable C++ parser" FORCE)
	set(ENABLE_JSON ON CACHE BOOL "Enable JSON" FORCE)
	set(ENABLE_OSP ON CACHE BOOL "Enable OSP" FORCE)
	list(APPEND Poco_COMPONENTS "OSP bundle creator")
endif()

if(ENABLE_OSP)
	set(ENABLE_UTIL ON CACHE BOOL "Enable Util" FORCE)
	set(ENABLE_XML ON CACHE BOOL "Enable XML" FORCE)
	set(ENABLE_ZIP ON CACHE BOOL "Enable Zip" FORCE)
endif()


# Poco project context for the module subdirectories, derived from this file's
# own location so superprojects just include() this module. The modules expect
# the platform tree as PROJECT_SOURCE_DIR, a Poco project name/version
# (POCO_GENERATE_PACKAGE writes Poco<Module>ConfigVersion.cmake), and
# SHARED_LIBRARY_VERSION for their sonames; without the latter the modules'
# VERSION/SOVERSION collapses and misnames the library libPoco<Module>.so.SOVERSION.
# The caller's values are restored at the end of this file.
get_filename_component(_pocoplus_root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
file(STRINGS "${_pocoplus_root}/VERSION" _pocoplus_version LIMIT_COUNT 1)
file(STRINGS "${_pocoplus_root}/libversion" _pocoplus_libversion LIMIT_COUNT 1)
set(_pocoplus_saved_project_source_dir "${PROJECT_SOURCE_DIR}")
set(_pocoplus_saved_project_name "${PROJECT_NAME}")
set(_pocoplus_saved_project_version "${PROJECT_VERSION}")
set(_pocoplus_saved_shared_library_version "${SHARED_LIBRARY_VERSION}")
set(PROJECT_SOURCE_DIR "${_pocoplus_root}")
set(PROJECT_NAME "Poco")
set(PROJECT_VERSION "${_pocoplus_version}")
set(SHARED_LIBRARY_VERSION "${_pocoplus_libversion}")

# OSP bundles are written to ${CMAKE_BINARY_DIR}/${POCO_INSTALL_BUNDLEDIR}, a
# runtime bundle-repository path; without it the com.appinf.osp.* bundles land
# in lib/ and are unresolvable at server startup.
if(NOT DEFINED POCO_INSTALL_BUNDLEDIR)
	set(POCO_INSTALL_BUNDLEDIR "bundles")
endif()

# add_subdirectory() needs an explicit binary directory when the platform tree
# is outside the top-level source (standalone component builds). Inside it
# (a superbuild), pass none so the binary layout stays source-mirrored.
string(FIND "${PROJECT_SOURCE_DIR}/" "${CMAKE_SOURCE_DIR}/" _pocoplus_prefix_pos)
macro(pocoplus_add_module _module)
	if(_pocoplus_prefix_pos EQUAL 0)
		add_subdirectory(${PROJECT_SOURCE_DIR}/${_module})
	else()
		add_subdirectory(${PROJECT_SOURCE_DIR}/${_module} ${CMAKE_BINARY_DIR}/pocoplus/${_module})
	endif()
endmacro()

if(ENABLE_GEO)
	pocoplus_add_module(Geo)
	list(APPEND Poco_COMPONENTS "Geo")
endif()

if(ENABLE_SERIAL)
	pocoplus_add_module(Serial)
	list(APPEND Poco_COMPONENTS "Serial")
endif()

if(ENABLE_WEBTUNNEL)
	pocoplus_add_module(WebTunnel)
	list(APPEND Poco_COMPONENTS "WebTunnel")
endif()

if(ENABLE_XSDVALIDATOR)
	# XSD Validator links LibXml2::LibXml2. A standalone platform build gets it from
	# the dependencies/libxml2 wrapper, which this setup does not add, so resolve it here.
	find_package(LibXml2 REQUIRED)
	pocoplus_add_module(XSD/Validator)
	list(APPEND Poco_COMPONENTS "XSDValidator")
endif()

if(ENABLE_JS)
	pocoplus_add_module(JS)
	list(APPEND Poco_COMPONENTS "Javascript with NodeJS")
endif()

if(ENABLE_REMOTING)
	pocoplus_add_module(CodeGeneration)
	list(APPEND Poco_COMPONENTS "CodeGeneration")

	pocoplus_add_module(RemotingNG)
	list(APPEND Poco_COMPONENTS "RemotingNG")

	pocoplus_add_module(RemotingNG/REST)
	list(APPEND Poco_COMPONENTS "RemotingNGREST")

	pocoplus_add_module(RemotingNG/TCP)
	list(APPEND Poco_COMPONENTS "RemotingNGTCP")
endif()

if(ENABLE_OSP)
	pocoplus_add_module(OSP)
	list(APPEND Poco_COMPONENTS "OSP")
endif()

if(ENABLE_OSP_AUTH)
	list(APPEND Poco_COMPONENTS "OSP/Auth")
endif()

# Restore the caller's project context.
set(PROJECT_SOURCE_DIR "${_pocoplus_saved_project_source_dir}")
set(PROJECT_NAME "${_pocoplus_saved_project_name}")
set(PROJECT_VERSION "${_pocoplus_saved_project_version}")
set(SHARED_LIBRARY_VERSION "${_pocoplus_saved_shared_library_version}")
unset(_pocoplus_saved_project_source_dir)
unset(_pocoplus_saved_project_name)
unset(_pocoplus_saved_project_version)
unset(_pocoplus_saved_shared_library_version)
unset(_pocoplus_version)
unset(_pocoplus_libversion)
unset(_pocoplus_root)
unset(_pocoplus_prefix_pos)

