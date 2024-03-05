add_executable(anariMPIDistribTutorialSpheres anariMPIDistribTutorialSpheres.cpp)
target_link_libraries(anariMPIDistribTutorialSpheres anari::anari MPI::MPI_CXX util glfw)
target_include_directories(anariMPIDistribTutorialSpheres PRIVATE ../util)
target_include_directories(anariMPIDistribTutorialSpheres SYSTEM PRIVATE ../external)
target_include_directories(
    anariMPIDistribTutorialSpheres SYSTEM PRIVATE
    ../external/imgui
    ../external/imgui/backends
)
