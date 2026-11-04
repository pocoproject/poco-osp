include(CMakeFindDependencyMacro)
find_dependency(PocoFoundation)
find_dependency(PocoOSP)
include("${CMAKE_CURRENT_LIST_DIR}/PocoOSPAuthTargets.cmake")
