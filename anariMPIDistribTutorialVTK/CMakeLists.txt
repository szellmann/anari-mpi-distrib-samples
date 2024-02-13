cmake_minimum_required(VERSION 3.14) # CMP0083 NEW

find_package(VTK COMPONENTS 
  CommonColor
  CommonCore
  CommonDataModel
  FiltersCore
  FiltersGeometry
  FiltersModeling
  InteractionStyle
  IOGeometry
  RenderingContextOpenGL2
  RenderingCore
  RenderingFreeType
  RenderingGL2PSOpenGL2
  RenderingOpenGL2
  RenderingAnari
)

if (NOT VTK_FOUND)
  message("Not building anariMPIDistribTutorialVTK: Unable to find VTK.")
  return()
endif()

include(CheckPIESupported)
check_pie_supported()

add_executable(anariMPIDistribTutorialVTK
    anariMPIDistribTutorialVTK.cpp
    vtkAnariPassMPI.cxx
)
set_target_properties(anariMPIDistribTutorialVTK
    PROPERTIES
    POSITION_INDEPENDENT_CODE FALSE
)
target_link_libraries(
    anariMPIDistribTutorialVTK
    anari::anari
    ${VTK_LIBRARIES}
    MPI::MPI_CXX util
)
target_include_directories(anariMPIDistribTutorialVTK PRIVATE ../util)
target_include_directories(anariMPIDistribTutorialVTK SYSTEM PRIVATE ../external)
# vtk_module_autoinit is needed
vtk_module_autoinit(
  TARGETS anariMPIDistribTutorialVTK
  MODULES ${VTK_LIBRARIES}
)
