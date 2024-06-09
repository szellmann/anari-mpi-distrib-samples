add_executable(anariMPIDistribTutorialTriangleMesh anariMPIDistribTutorialTriangleMesh.cpp)
target_link_libraries(anariMPIDistribTutorialTriangleMesh anari::anari MPI::MPI_CXX)
target_include_directories(anariMPIDistribTutorialTriangleMesh PRIVATE ../util)
target_include_directories(anariMPIDistribTutorialTriangleMesh SYSTEM PRIVATE ../external)
option(ENABLE_ICET "Enable *only* if you want the app itself to do compositing" OFF)
if(${ENABLE_ICET})
  find_package(IceT REQUIRED)
  target_compile_definitions(anariMPIDistribTutorialTriangleMesh PRIVATE WITH_ICET=1)
  target_include_directories(anariMPIDistribTutorialTriangleMesh PRIVATE ${ICET_INCLUDE_DIRS})
  target_link_libraries(anariMPIDistribTutorialTriangleMesh
    ${ICET_CORE_LIBS}
    ${ICET_MPI_LIBS}
    ${ICET_LIBARIES})
endif()
