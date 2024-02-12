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
  message("Not building anariMPIDistribTutorialVTKSimple: Unable to find VTK.")
  return()
endif()

add_executable(anariMPIDistribTutorialVTKSimple anariMPIDistribTutorialVTKSimple.cpp)
target_link_libraries(
    anariMPIDistribTutorialVTKSimple
    anari::anari
    ${VTK_LIBRARIES}
    MPI::MPI_CXX util
)
target_include_directories(anariMPIDistribTutorialVTKSimple PRIVATE ../util)
target_include_directories(anariMPIDistribTutorialVTKSimple SYSTEM PRIVATE ../external)
# vtk_module_autoinit is needed
vtk_module_autoinit(
  TARGETS anariMPIDistribTutorialVTKSimple
  MODULES ${VTK_LIBRARIES}
)
