
#include "GLFWDistribANARIWindow.h"
#include <imgui.h>
#include <mpi.h>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include "imgui_impl_glfw_gl3.h"

using namespace anari;
using namespace util;
using namespace anari::math;

template <typename R, typename TASK_T>
static std::future<R> async(TASK_T &&fcn)
{
  auto task = std::packaged_task<R()>(std::forward<TASK_T>(fcn));
  auto future = task.get_future();

  std::thread([task = std::move(task)]() mutable { task(); }).detach();

  return future;
}

template <typename R>
static bool is_ready(const std::future<R> &f)
{
  return !f.valid()
      || f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

GLFWDistribANARIWindow *GLFWDistribANARIWindow::activeWindow = nullptr;

static bool g_quitNextFrame = false;

WindowState::WindowState()
    : quit(false),
      cameraChanged(false),
      fbSizeChanged(false),
      spp(1),
      windowSize(0),
      eyePos(0.f),
      lookDir(0.f),
      upDir(0.f)
{}

GLFWDistribANARIWindow::GLFWDistribANARIWindow(const int2 &windowSize,
    const box3 &worldBounds,
    anari::Device device,
    anari::World world,
    anari::Renderer renderer)
    : windowSize(windowSize),
      worldBounds(worldBounds),
      device(device),
      world(world),
      renderer(renderer)
{
  MPI_Comm_rank(MPI_COMM_WORLD, &mpiRank);
  MPI_Comm_size(MPI_COMM_WORLD, &mpiWorldSize);

  if (mpiRank == 0) {
    if (activeWindow != nullptr) {
      throw std::runtime_error(
          "Cannot create more than one GLFWDistribANARIWindow!");
    }

    activeWindow = this;

    // initialize GLFW
    if (!glfwInit()) {
      throw std::runtime_error("Failed to initialize GLFW!");
    }

    // create GLFW window
    glfwWindow = glfwCreateWindow(
        windowSize.x, windowSize.y, "ANARI Tutorial", NULL, NULL);

    if (!glfwWindow) {
      glfwTerminate();
      throw std::runtime_error("Failed to create GLFW window!");
    }

    // make the window's context current
    glfwMakeContextCurrent(glfwWindow);

    ImGui_ImplGlfwGL3_Init(glfwWindow, true);

    // set initial OpenGL state
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);

    // create OpenGL frame buffer texture
    glGenTextures(1, &framebufferTexture);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, framebufferTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // set GLFW callbacks
    glfwSetFramebufferSizeCallback(
        glfwWindow, [](GLFWwindow *, int newWidth, int newHeight) {
          activeWindow->reshape(int2{newWidth, newHeight});
        });

    glfwSetCursorPosCallback(glfwWindow, [](GLFWwindow *, double x, double y) {
      ImGuiIO &io = ImGui::GetIO();
      if (!io.WantCaptureMouse) {
        activeWindow->motion(int2{int(x), int(y)});
      }
    });

    glfwSetKeyCallback(
        glfwWindow, [](GLFWwindow *, int key, int, int action, int) {
          if (action == GLFW_PRESS) {
            switch (key) {
            case GLFW_KEY_G:
              activeWindow->showUi = !(activeWindow->showUi);
              break;
            case GLFW_KEY_Q:
              g_quitNextFrame = true;
              break;
            }
          }
        });
  }

  // ANARI setup

  // create the arcball camera model
  arcballCamera = std::unique_ptr<ArcballCamera>(
      new ArcballCamera(worldBounds, windowSize));

  // create camera
  camera = anari::newObject<anari::Camera>(device, "perspective");
  anari::setParameter(device, camera, "aspect", windowSize.x / float(windowSize.y));
  anari::setParameter(device, camera, "position", arcballCamera->getEye());
  anari::setParameter(device, camera, "direction",
      arcballCamera->getCenter() - arcballCamera->getEye());
  anari::setParameter(device, camera, "up", arcballCamera->getUp());
  anari::commitParameters(device, camera);

  anari::commitParameters(device, renderer);

  windowState.windowSize = windowSize;
  windowState.eyePos = arcballCamera->getEye();
  windowState.lookDir = arcballCamera->getCenter() - arcballCamera->getEye();
  windowState.upDir = arcballCamera->getUp();

  if (mpiRank == 0) {
    // trigger window reshape events with current window size
    glfwGetFramebufferSize(
        glfwWindow, &this->windowSize.x, &this->windowSize.y);
    reshape(this->windowSize);
  }
}

GLFWDistribANARIWindow::~GLFWDistribANARIWindow()
{
  if (mpiRank == 0) {
    ImGui_ImplGlfwGL3_Shutdown();
    // cleanly terminate GLFW
    glfwTerminate();
  }
}

GLFWDistribANARIWindow *GLFWDistribANARIWindow::getActiveWindow()
{
  return activeWindow;
}

anari::World GLFWDistribANARIWindow::getWorld()
{
  return world;
}

void GLFWDistribANARIWindow::setWorld(anari::World newWorld)
{
  world = newWorld;
  addObjectToCommit(world);
}

void GLFWDistribANARIWindow::resetAccumulation()
{
}

void GLFWDistribANARIWindow::registerDisplayCallback(
    std::function<void(GLFWDistribANARIWindow *)> callback)
{
  displayCallback = callback;
}

void GLFWDistribANARIWindow::registerImGuiCallback(
    std::function<void()> callback)
{
  uiCallback = callback;
}

void GLFWDistribANARIWindow::mainLoop()
{
  while (true) {
    MPI_Bcast(&windowState, sizeof(WindowState), MPI_BYTE, 0, MPI_COMM_WORLD);
    if (windowState.quit) {
      break;
    }

    // TODO: Actually render asynchronously, if we have MPI thread multiple
    // support
    startNewANARIFrame();
    waitOnANARIFrame();

    // if a display callback has been registered, call it
    if (displayCallback) {
      displayCallback(this);
    }

    if (mpiRank == 0) {
      ImGui_ImplGlfwGL3_NewFrame();

      display();

      // poll and process events
      glfwPollEvents();
      windowState.quit = glfwWindowShouldClose(glfwWindow) || g_quitNextFrame;
    }
  }
}

void GLFWDistribANARIWindow::reshape(const int2 &newWindowSize)
{
  windowSize = newWindowSize;
  windowState.windowSize = windowSize;
  windowState.fbSizeChanged = true;

  // update camera
  arcballCamera->updateWindowSize(windowSize);

  // reset OpenGL viewport and orthographic projection
  glViewport(0, 0, windowSize.x, windowSize.y);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0.0, windowSize.x, 0.0, windowSize.y, -1.0, 1.0);
}

void GLFWDistribANARIWindow::motion(const int2 &position)
{
  arcballCamera->handleMouseEvent(position.x, position.y, glfwWindow);

  if (arcballCamera->hasCameraChanged()) {
    windowState.cameraChanged = true;
    windowState.eyePos = arcballCamera->getEye();
    windowState.lookDir = arcballCamera->getCenter() - arcballCamera->getEye();
    windowState.upDir = arcballCamera->getUp();
  }
}

void GLFWDistribANARIWindow::display()
{
  // clock used to compute frame rate
  static auto displayStart = std::chrono::high_resolution_clock::now();

  if (showUi && uiCallback) {
    ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize;
    ImGui::Begin(
        "Tutorial Controls (press 'g' to hide / show)", nullptr, flags);
    uiCallback();

    ImGui::End();
  }

  // updateTitleBar();


  static bool firstFrame = true;
  if (firstFrame || is_ready(currentFrame)) {
    // display frame rate in window title
    auto displayEnd = std::chrono::high_resolution_clock::now();
    auto durationMilliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            displayEnd - displayStart);

    latestFPS = 1000.f / float(durationMilliseconds.count());

    // map ANARI frame buffer, update OpenGL texture with its contents, then
    // unmap

    auto fb = anari::map<uint32_t>(device, frame, "channel.color");

    glBindTexture(GL_TEXTURE_2D, framebufferTexture);
    glTexImage2D(GL_TEXTURE_2D,
        0,
        GL_RGBA,
        windowSize.x,
        windowSize.y,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        fb.data);

    anari::unmap(device, frame, "channel.color");

    // Start new frame and reset frame timing interval start
    displayStart = std::chrono::high_resolution_clock::now();
    firstFrame = false;
  }

  // clear current OpenGL color buffer
  glClear(GL_COLOR_BUFFER_BIT);

  // render textured quad with OSPRay frame buffer contents
  glBegin(GL_QUADS);

  glTexCoord2f(0.f, 0.f);
  glVertex2f(0.f, 0.f);

  glTexCoord2f(0.f, 1.f);
  glVertex2f(0.f, windowSize.y);

  glTexCoord2f(1.f, 1.f);
  glVertex2f(windowSize.x, windowSize.y);

  glTexCoord2f(1.f, 0.f);
  glVertex2f(windowSize.x, 0.f);

  glEnd();

  ImGui::Render();
  ImGui_ImplGlfwGL3_Render();

  // swap buffers
  glfwSwapBuffers(glfwWindow);
}

void GLFWDistribANARIWindow::startNewANARIFrame()
{
  //currentFrame = async<void>([&, this]() {
    bool fbNeedsClear = false;
    auto handles = objectsToCommit.consume();
    if (!handles.empty()) {
      for (auto &h : handles)
        anari::commitParameters(device, h);

      fbNeedsClear = true;
    }

    if (windowState.fbSizeChanged) {
      windowState.fbSizeChanged = false;
      windowSize = windowState.windowSize;

      frame = anari::newObject<anari::Frame>(device);
      anari::setParameter(device, frame, "size", (uint2)windowSize);
      anari::setParameter(device, frame, "channel.color", ANARI_UFIXED8_RGBA_SRGB);
      anari::setParameter(device, frame, "world", world);
      anari::setParameter(device, frame, "renderer", renderer);
      anari::setParameter(device, frame, "camera", camera);
      anari::commitParameters(device, frame);

      anari::setParameter(device, camera, "aspect", windowSize.x / float(windowSize.y));
      anari::commitParameters(device, camera);

      fbNeedsClear = true;
    }
    if (windowState.cameraChanged) {
      windowState.cameraChanged = false;
      anari::setParameter(device, camera, "aspect", windowSize.x / float(windowSize.y));
      anari::setParameter(device, camera, "position", windowState.eyePos);
      anari::setParameter(device, camera, "direction", windowState.lookDir);
      anari::setParameter(device, camera, "up", windowState.upDir);
      anari::commitParameters(device, camera);
      fbNeedsClear = true;
    }

    if (fbNeedsClear) {
      resetAccumulation();
    }

    anari::render(device, frame);
  //});
}

void GLFWDistribANARIWindow::waitOnANARIFrame()
{
  // if (currentFrame.valid()) {
  //   currentFrame.get();
  // }
}

void GLFWDistribANARIWindow::addObjectToCommit(ANARIObject obj)
{
  objectsToCommit.push_back(obj);
}

void GLFWDistribANARIWindow::updateTitleBar()
{
  std::stringstream windowTitle;
  windowTitle << "ANARI: " << std::setprecision(3) << latestFPS << " fps";
  if (latestFPS < 2.f) {
    float progress = 0.f;//currentFrame.progress();
    windowTitle << " | ";
    int barWidth = 20;
    std::string progBar;
    progBar.resize(barWidth + 2);
    auto start = progBar.begin() + 1;
    auto end = start + progress * barWidth;
    std::fill(start, end, '=');
    std::fill(end, progBar.end(), '_');
    *end = '>';
    progBar.front() = '[';
    progBar.back() = ']';
    windowTitle << progBar;
  }

  glfwSetWindowTitle(glfwWindow, windowTitle.str().c_str());
}
