include(CMakeFindDependencyMacro)
find_dependency(PocoOSP)
find_dependency(PocoCrypto)
include("${CMAKE_CURRENT_LIST_DIR}/PocoOSPAuthDataTargets.cmake")
