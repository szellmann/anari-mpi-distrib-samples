// https://github.com/ospray/ospray/blob/master/modules/mpi/tutorials/ospMPIDistribTutorial.cpp

#include <errno.h>
#include <mpi.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "anari/anari_cpp.hpp"
#include "anari/anari_cpp/ext/linalg.h"

using namespace anari::math;

static void statusFunc(const void * /*userData*/,
    ANARIDevice /*device*/,
    ANARIObject source,
    ANARIDataType /*sourceType*/,
    ANARIStatusSeverity severity,
    ANARIStatusCode /*code*/,
    const char *message)
{
  if (severity == ANARI_SEVERITY_FATAL_ERROR) {
    fprintf(stderr, "[FATAL][%p] %s\n", source, message);
    std::exit(1);
  } else if (severity == ANARI_SEVERITY_ERROR) {
    fprintf(stderr, "[ERROR][%p] %s\n", source, message);
  } else if (severity == ANARI_SEVERITY_WARNING) {
    fprintf(stderr, "[WARN ][%p] %s\n", source, message);
  } else if (severity == ANARI_SEVERITY_PERFORMANCE_WARNING) {
    fprintf(stderr, "[PERF ][%p] %s\n", source, message);
  }
  // Ignore INFO/DEBUG messages
}

int main(int argc, char **argv)
{
  int mpiThreadCapability = 0;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &mpiThreadCapability);
  if (mpiThreadCapability != MPI_THREAD_MULTIPLE
      && mpiThreadCapability != MPI_THREAD_SERIALIZED) {
    fprintf(stderr,
        "OSPRay requires the MPI runtime to support thread "
        "multiple or thread serialized.\n");
    return 1;
  }

  int mpiRank = 0;
  int mpiWorldSize = 0;
  MPI_Comm_rank(MPI_COMM_WORLD, &mpiRank);
  MPI_Comm_size(MPI_COMM_WORLD, &mpiWorldSize);

  // image size
  uint2 imgSize;
  imgSize.x = 1024; // width
  imgSize.y = 768; // height

  // camera
  float3 cam_pos{(mpiWorldSize + 1.f) / 2.f, 0.5f, -mpiWorldSize * 0.5f};
  float3 cam_up{0.f, 1.f, 0.f};
  float3 cam_view{0.f, 0.f, 1.f};

  // all ranks specify the same rendering parameters, with the exception of
  // the data to be rendered, which is distributed among the ranks
  // triangle mesh data
  float3 vertex[] = {float3(mpiRank, 0.0f, 3.5f),
      float3(mpiRank, 1.0f, 3.0f),
      float3(1.0f * (mpiRank + 1.f), 0.0f, 3.0f),
      float3(1.0f * (mpiRank + 1.f), 0.0f, 2.5f)};
  float4 color[] = {float4(0.0f, 0.0f, (mpiRank + 1.f) / mpiWorldSize, 1.0f),
      float4(0.0f, 0.0f, (mpiRank + 1.f) / mpiWorldSize, 1.0f),
      float4(0.0f, 0.0f, (mpiRank + 1.f) / mpiWorldSize, 1.0f),
      float4(0.0f, 0.0f, (mpiRank + 1.f) / mpiWorldSize, 1.0f)};
  uint3 index[] = {uint3(0, 1, 2), uint3(1, 2, 3)};

  // load "barney", an MPI distributed anari device
  auto library = anari::loadLibrary("barney", statusFunc);

  // use scoped lifetimes of wrappers to release everything before unloadLibrary()
  {
    auto device = anari::newDevice(library, "default");
    anari::commitParameters(device, device);

    // create and setup camera
    auto camera = anari::newObject<anari::Camera>(device, "perspective");
    anari::setParameter(device, camera, "aspect", imgSize.x / (float)imgSize.y);
    anari::setParameter(device, camera, "position", cam_pos);
    anari::setParameter(device, camera, "direction", cam_view);
    anari::setParameter(device, camera, "up", cam_up);
    anari::commitParameters(device, camera); // commit each object to indicate modifications are done

    // create and setup model and mesh
    auto mesh = anari::newObject<anari::Geometry>(device, "triangle");
    anari::Array1D data;
    data = anari::newArray1D(device, vertex, 4);
    anari::setAndReleaseParameter(device, mesh, "vertex.position", data);

    data = anari::newArray1D(device, color, 4);
    anari::setAndReleaseParameter(device, mesh, "vertex.color", data);

    data = anari::newArray1D(device, index, 2);
    anari::setAndReleaseParameter(device, mesh, "primitive.index", data);

    anari::commitParameters(device, mesh);

    // put the mesh into a surface
    auto surface = anari::newObject<anari::Surface>(device);
    anari::setParameter(device, surface, "geometry", mesh);
    anari::setAndReleaseParameter(device, surface, "material",
        anari::newObject<anari::Material>(device, "matte"));
    anari::commitParameters(device, surface);

    // put the surface into a group (collection of surfaces)
    auto group = anari::newObject<anari::Group>(device);
    anari::setParameterArray1D(device, group, "surface", &surface, 1);
    anari::commitParameters(device, group);

    // put the group into an instance (give the group a world transform)
    auto instance = anari::newObject<anari::Instance>(device, "transform");
    anari::setParameterArray1D(device, instance, "group", &group, 1);
    anari::commitParameters(device, instance);

    auto world = anari::newObject<anari::World>(device);
    //anari::setParameterArray1D(device, world, "instance", &instance, 1); // TODO: doesn't work?!
    anari::setParameterArray1D(device, world, "surface", &surface, 1);

    // Specify the region of the world this rank owns
    //float3 regionBounds[] = {
    //    float3(mpiRank, 0.f, 2.5f), float3(1.f * (mpiRank + 1.f), 1.f, 3.5f)};
    //anari::setParameter(device, world, "region", regionBounds);

    anari::commitParameters(device, world);

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

    anari::release(device, renderer);
    anari::release(device, world);
    anari::release(device, frame);
    anari::release(device, device);
  }

  anari::unloadLibrary(library);

  MPI_Finalize();

  return 0;
}
