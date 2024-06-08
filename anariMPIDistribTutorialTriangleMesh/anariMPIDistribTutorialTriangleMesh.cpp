// https://github.com/ospray/ospray/blob/master/modules/mpi/tutorials/ospMPIDistribTutorial.cpp

#include <errno.h>
#include <mpi.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "anari/anari_cpp.hpp"
#include "anari/anari_cpp/ext/linalg.h"

#include "statusFunc.h"
#include "PartitionedMeshLoader.h"
#include "Camera.h"

using namespace anari::math;

inline float3 randomColor(unsigned idx)
{
  unsigned int r = (unsigned int)(idx*13*17 + 0x234235);
  unsigned int g = (unsigned int)(idx*7*3*5 + 0x773477);
  unsigned int b = (unsigned int)(idx*11*19 + 0x223766);
  return float3((r&255)/255.f,
                (g&255)/255.f,
                (b&255)/255.f);
}

int main(int argc, char **argv)
{
  int mpiThreadCapability = 0;
  MPI_Init(&argc, &argv);

  int mpiRank = 0;
  int mpiWorldSize = 0;
  MPI_Comm_rank(MPI_COMM_WORLD, &mpiRank);
  MPI_Comm_size(MPI_COMM_WORLD, &mpiWorldSize);

  auto library = anari::loadLibrary("environment", statusFunc);

  auto device = anari::newDevice(library, "default");
  anari::commitParameters(device, device);

  box3 bounds;
  util::PartitionedMeshLoader loader;
  auto anariGeoms = loader.loadANARI(
      device, "/Users/stefan/anari-mpi-distrib-samples/build/teapot.tri", mpiRank, mpiWorldSize, &bounds);

  std::vector<anari::Surface> surfaces;
  for (auto geom : anariGeoms) {
    auto surface = anari::newObject<anari::Surface>(device);
    anari::setParameter(device, surface, "geometry", geom);
    auto material = anari::newObject<anari::Material>(device, "matte");
    anari::setParameter(device, material, "color", randomColor(mpiRank));
    anari::commitParameters(device, material);
    anari::setParameter(device, surface, "material", material);
    anari::commitParameters(device, surface);
    surfaces.push_back(surface);
  }

  auto world = anari::newObject<anari::World>(device);
  auto surfs = anari::newArray1D(device, surfaces.data(), surfaces.size());
  anari::setAndReleaseParameter(device, world, "surface", surfs);
  anari::commitParameters(device, world);

  // image size
  uint2 imgSize;
  imgSize.x = 1024; // width
  imgSize.y = 768; // height

  // create and setup camera
  util::Camera cam;
  cam.perspective(
      60.f*util::Camera::deg2rad, imgSize.x / (float)imgSize.y, 0.0001f, 10000.f);
  cam.viewAll(bounds);

   // std::cout << cam.getEye().x << ',' << cam.getEye().y << ',' << cam.getEye().z << '\n';
   // std::cout << cam.getCenter().x << ',' << cam.getCenter().y << ',' << cam.getCenter().z << '\n';
   // std::cout << cam.getUp().x << ',' << cam.getUp().y << ',' << cam.getUp().z << '\n';

  auto camera = anari::newObject<anari::Camera>(device, "perspective");
  anari::setParameter(device, camera, "aspect", imgSize.x / (float)imgSize.y);
  anari::setParameter(device, camera, "position", cam.getEye());
  anari::setParameter(device, camera, "direction", cam.getCenter() - cam.getEye());
  anari::setParameter(device, camera, "up", cam.getUp());
  anari::commitParameters(device, camera); // commit each object to indicate modifications are done

  // create the default renderer
  auto renderer = anari::newObject<anari::Renderer>(device, "default");
  anari::commitParameters(device, renderer);

  // create and setup frame
  auto frame = anari::newObject<anari::Frame>(device);
  anari::setParameter(device, frame, "size", imgSize);
  anari::setParameter(device, frame, "channel.color", ANARI_UFIXED8_RGBA_SRGB);
  anari::setParameter(device, frame, "world", world);
  anari::setParameter(device, frame, "renderer", renderer);
  anari::setParameter(device, frame, "camera", camera);
  anari::commitParameters(device, frame);

  // render one frame
  anari::render(device, frame);

  // on rank 0, access framebuffer and write its content as PNG file
  if (mpiRank == 0) {
    auto fb = anari::map<uint32_t>(device, frame, "channel.color");
    stbi_flip_vertically_on_write(1);
    stbi_write_png(
        "firstFrameCpp.png", imgSize.x, imgSize.y, 4, fb.data, 4 * fb.width);
    anari::unmap(device, frame, "channel.color");
  }

  // render 10 more frames, which are accumulated to result in a better
  // converged image
  for (int frames = 0; frames < 10; frames++)
    anari::render(device, frame);

  if (mpiRank == 0) {
    auto fb = anari::map<uint32_t>(device, frame, "channel.color");
    stbi_write_png(
        "accumulatedFrameCpp.png", imgSize.x, imgSize.y, 4, fb.data, 4 * fb.width);
    anari::unmap(device, frame, "channel.color");
  }

  MPI_Finalize();

  return 0;
}
