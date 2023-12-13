cmake_minimum_required(VERSION 3.10)
project(anari_mpi_distrib_samles LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(anari REQUIRED)
find_package(MPI REQUIRED)

add_subdirectory(anariMPIDistribTutorial)
