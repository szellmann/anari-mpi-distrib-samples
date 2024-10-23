// https://github.com/ospray/ospray/blob/master/modules/mpi/tutorials/ospMPIDistribTutorialSpheres.cpp

/* This larger example shows how to use the MPIDistributedDevice to write an
 * interactive rendering application, which shows a UI on rank 0 and uses
 * all ranks in the MPI world for data loading and rendering. Each rank
 * generates a local sub-piece of spheres data, e.g., as if rendering some
 * large distributed dataset.
 */

#include <imgui.h>
#include <mpi.h>
#include <array>
#include <iostream>
#include <iterator>
#include <random>
#include "GLFWDistribANARIWindow.h"
#include "anari/anari_cpp.hpp"
#include "anari/anari_cpp/ext/linalg.h"
#include "statusFunc.h"

using namespace anari;
using namespace util;
using namespace anari::math;

// Generate the rank's local spheres within its assigned grid cell, and
// return the bounds of this grid cell
anari::Surface makeLocalSpheres(
    anari::Device device, const int mpiRank, const int mpiWorldSize, box3 &bounds);

int main(int argc, char **argv)
{
  int mpiThreadCapability = 0;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &mpiThreadCapability);
  if (mpiThreadCapability != MPI_THREAD_MULTIPLE
      && mpiThreadCapability != MPI_THREAD_SERIALIZED) {
    fprintf(stderr,
        "ANARI requires the MPI runtime to support thread "
        "multiple or thread serialized.\n");
    return 1;
  }

  int mpiRank = 0;
  int mpiWorldSize = 0;
  MPI_Comm_rank(MPI_COMM_WORLD, &mpiRank);
  MPI_Comm_size(MPI_COMM_WORLD, &mpiWorldSize);

  std::cout << "ANARI rank " << mpiRank << "/" << mpiWorldSize << "\n";

  auto library = anari::loadLibrary("environment", statusFunc);

  auto device = anari::newDevice(library, "default");
  anari::commitParameters(device, device);

  
  // all ranks specify the same rendering parameters, with the exception of
  // the data to be rendered, which is distributed among the ranks
  box3 regionBounds;
  anari::Surface spheres =
      makeLocalSpheres(device, mpiRank, mpiWorldSize, regionBounds);

  auto world = anari::newObject<anari::World>(device);
  anari::setParameterArray1D(device, world, "surface", &spheres, 1);

  // create ANARI renderer
  auto renderer = anari::newObject<anari::Renderer>(device, "default");

  // create and setup a directional light
  auto light = anari::newObject<anari::Light>(device, "directional");
  anari::setParameter(device, light, "direction", float3(-1.f, -1.f, 0.5f));
  anari::commitParameters(device, light);
  anari::setParameterArray1D(device, world, "light", &light, 1);

  anari::commitParameters(device, world);

  // create a GLFW OSPRay window: this object will create and manage the
  // OSPRay frame buffer and camera directly
  auto glfwANARIWindow =
      std::unique_ptr<GLFWDistribANARIWindow>(new GLFWDistribANARIWindow(
          int2{1024, 768}, box3(float3(-1.f), float3(1.f)), device, world, renderer));

  int spp = 1;
  int currentSpp = 1;
  if (mpiRank == 0) {
    glfwANARIWindow->registerImGuiCallback(
        [&]() { ImGui::SliderInt("pixelSamples", &spp, 1, 64); });
  }

  glfwANARIWindow->registerDisplayCallback(
        [&](GLFWDistribANARIWindow *win) {
          // Send the UI changes out to the other ranks so we can synchronize
          // how many samples per-pixel we're taking
          MPI_Bcast(&spp, 1, MPI_INT, 0, MPI_COMM_WORLD);
          if (spp != currentSpp) {
            currentSpp = spp;
            anari::setParameter(device, renderer, "pixelSamples", spp);
            win->addObjectToCommit(renderer);
          }
        });

  // start the GLFW main loop, which will continuously render
  glfwANARIWindow->mainLoop();

  MPI_Finalize();

  return 0;
}

bool computeDivisor(int x, int &divisor)
{
  int upperBound = std::sqrt(x);
  for (int i = 2; i <= upperBound; ++i) {
    if (x % i == 0) {
      divisor = i;
      return true;
    }
  }
  return false;
}

// Compute an X x Y x Z grid to have 'num' grid cells,
// only gives a nice grid for numbers with even factors since
// we don't search for factors of the number, we just try dividing by two
int3 computeGrid(int num)
{
  int3 grid(1);
  int axis = 0;
  int divisor = 0;
  while (computeDivisor(num, divisor)) {
    grid[axis] *= divisor;
    num /= divisor;
    axis = (axis + 1) % 3;
  }
  if (num != 1) {
    grid[axis] *= num;
  }
  return grid;
}

anari::Surface makeLocalSpheres(
    anari::Device device, const int mpiRank, const int mpiWorldSize, box3 &bounds)
{
  const float sphereRadius = 0.1f;
  std::vector<float> sphereRadii(50, sphereRadius);
  std::vector<float3> spheres(50);

  // To simulate loading a shared dataset all ranks generate the same
  // sphere data.
  std::random_device rd;
  std::mt19937 rng(rd());

  const int3 grid = computeGrid(mpiWorldSize);
  const int3 brickId(mpiRank % grid.x,
      (mpiRank / grid.x) % grid.y,
      mpiRank / (grid.x * grid.y));

  // The grid is over the [-1, 1] box
  const float3 brickSize = float3(2.0) / float3(grid);
  const float3 brickLower = brickSize * brickId - float3(1.f);
  const float3 brickUpper = brickSize * brickId - float3(1.f) + brickSize;

  bounds.lower = brickLower;
  bounds.upper = brickUpper;

  // Generate spheres within the box padded by the radius, so we don't need
  // to worry about ghost bounds
  std::uniform_real_distribution<float> distX(
      brickLower.x + sphereRadius, brickUpper.x - sphereRadius);
  std::uniform_real_distribution<float> distY(
      brickLower.y + sphereRadius, brickUpper.y - sphereRadius);
  std::uniform_real_distribution<float> distZ(
      brickLower.z + sphereRadius, brickUpper.z - sphereRadius);

  for (auto &s : spheres) {
    s.x = distX(rng);
    s.y = distY(rng);
    s.z = distZ(rng);
  }

  auto sphereGeom = anari::newObject<anari::Geometry>(device, "sphere");
  anari::setParameterArray1D(device, sphereGeom, "vertex.radius", sphereRadii.data(), sphereRadii.size());
  anari::setParameterArray1D(device, sphereGeom, "vertex.position", spheres.data(), spheres.size());
  anari::commitParameters(device, sphereGeom);

  float3 color(0.f, 0.f, (mpiRank + 1.f) / mpiWorldSize);
  auto material = anari::newObject<anari::Material>(device, "matte");
  anari::setParameter(device, material, "color", color);
  anari::commitParameters(device, material);

  auto surface = anari::newObject<anari::Surface>(device);
  anari::setAndReleaseParameter(device, surface, "geometry", sphereGeom);
  anari::setAndReleaseParameter(device, surface, "material", material);
  anari::commitParameters(device, surface);

  return surface;
}
