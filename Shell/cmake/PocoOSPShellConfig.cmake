include(CMakeFindDependencyMacro)
find_dependency(PocoOSP)
find_dependency(PocoUtil)
include("${CMAKE_CURRENT_LIST_DIR}/PocoOSPShellTargets.cmake")
