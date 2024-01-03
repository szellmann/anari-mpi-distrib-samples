
#pragma once

#include <GLFW/glfw3.h>
#include <functional>
#include <future>
#include <memory>
#include "ArcballCamera.h"
#include "TransactionalBuffer.h"
// anari
#include "anari/anari_cpp.hpp"

struct WindowState
{
  bool quit;
  bool cameraChanged;
  bool fbSizeChanged;
  int spp;
  anari::math::int2 windowSize;
  anari::math::float3 eyePos;
  anari::math::float3 lookDir;
  anari::math::float3 upDir;

  WindowState();
};

class GLFWDistribANARIWindow
{
 public:
  GLFWDistribANARIWindow(const anari::math::int2 &windowSize,
      const anari::math::box3 &worldBounds,
      anari::Device device,
      anari::World world,
      anari::Renderer renderer);

  ~GLFWDistribANARIWindow();

  static GLFWDistribANARIWindow *getActiveWindow();

  anari::World getWorld();
  void setWorld(anari::World newWorld);

  void resetAccumulation();

  void registerDisplayCallback(
      std::function<void(GLFWDistribANARIWindow *)> callback);

  void registerImGuiCallback(std::function<void()> callback);

  void mainLoop();

  void addObjectToCommit(ANARIObject obj);

 protected:
  void reshape(const anari::math::int2 &newWindowSize);
  void motion(const anari::math::int2 &position);
  void display();
  void startNewANARIFrame();
  void waitOnANARIFrame();
  void updateTitleBar();

  static GLFWDistribANARIWindow *activeWindow;

  anari::math::int2 windowSize;
  anari::math::box3 worldBounds;
  anari::Device device = nullptr;
  anari::World world = nullptr;
  anari::Renderer renderer = nullptr;

  int mpiRank = -1;
  int mpiWorldSize = -1;

  // GLFW window instance
  GLFWwindow *glfwWindow = nullptr;

  // Arcball camera instance
  std::unique_ptr<util::ArcballCamera> arcballCamera;

  // ANARI objects managed by this class
  anari::Camera camera = nullptr;
  anari::Frame frame = nullptr;

  std::future<void> currentFrame;

  // List of ANARI handles to commit before the next frame
  util::containers::TransactionalBuffer<ANARIObject> objectsToCommit;

  // OpenGL framebuffer texture
  GLuint framebufferTexture = 0;

  // optional registered display callback, called before every display()
  std::function<void(GLFWDistribANARIWindow *)> displayCallback;

  // toggles display of ImGui UI, if an ImGui callback is provided
  bool showUi = true;

  // optional registered ImGui callback, called during every frame to build UI
  std::function<void()> uiCallback;

  // FPS measurement of last frame
  float latestFPS{0.f};

  // The window state to be sent out over MPI to the other rendering processes
  WindowState windowState;
};
