include(CMakeFindDependencyMacro)
find_dependency(PocoOSP)
find_dependency(PocoNet)
include("${CMAKE_CURRENT_LIST_DIR}/PocoOSPAuthTargets.cmake")
