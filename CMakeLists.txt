cmake_minimum_required(VERSION 3.10)
project(anari_mpi_distrib_samles LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(anari REQUIRED)
find_package(MPI REQUIRED)
find_package(glfw3 REQUIRED)
set(OpenGL_GL_PREFERENCE "LEGACY")
find_package(OpenGL 2 REQUIRED)

add_library(util STATIC
  external/imgui/imgui.cpp
  external/imgui/imgui_draw.cpp
  external/imgui/imgui_tables.cpp
  external/imgui/imgui_widgets.cpp
  util/ArcballManip.cpp
  util/PanManip.cpp
  util/ZoomManip.cpp
  util/GLFWDistribANARIWindow.cpp
  util/imgui_impl_glfw_gl3.cpp
)
target_link_libraries(util PRIVATE anari::anari glfw MPI::MPI_CXX ${OPENGL_LIBRARIES})
target_include_directories(util SYSTEM PRIVATE external/imgui)

add_subdirectory(anariMPIDistribTutorial)
add_subdirectory(anariMPIDistribTutorialSpheres)
