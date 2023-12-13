add_executable(anariMPIDistribTutorial anariMPIDistribTutorial.cpp)
target_link_libraries(anariMPIDistribTutorial anari::anari MPI::MPI_CXX)
target_include_directories(anariMPIDistribTutorial SYSTEM PRIVATE ../external)
