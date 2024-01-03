
#pragma once

// glfw
#include <GLFW/glfw3.h>
// anari
#include "anari/anari_cpp/ext/linalg.h"
// ours
#include "ArcballManip.h"

namespace util {
  struct ArcballCamera : Camera
  {
    using int2 = anari::math::int2;

    ArcballCamera(Camera::box3 worldBounds, int2 winSize)
      : manip((Camera &)*this)
    {
      viewAll(worldBounds);
      manip.resize(winSize.x, winSize.y);
    }

    void updateWindowSize(int2 winSize)
    {
      manip.resize(winSize.x, winSize.y);
    }

    void handleMouseEvent(int x, int y, GLFWwindow *glfwWindow)
    {
      const bool leftDown =
        glfwGetMouseButton(glfwWindow, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
      const bool rightDown =
        glfwGetMouseButton(glfwWindow, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
      const bool middleDown =
        glfwGetMouseButton(glfwWindow, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;

      cameraChanged = leftDown || rightDown || middleDown;

      static bool g_leftDown = false, g_rightDown = false, g_middleDown = false;

      ArcballManip::ModifierKey mod = ArcballManip::None; // TODO!

      // left
      if (leftDown && !g_leftDown) {
        manip.handleMouseDown(x, y, ArcballManip::Left, mod);
      } else if (leftDown && g_leftDown) {
        manip.handleMouseMove(x, y, ArcballManip::Left, mod);
      } else if (!leftDown && g_leftDown) {
        manip.handleMouseUp(x, y, ArcballManip::Left, mod);
      }

      // right
      if (rightDown && !g_rightDown) {
        manip.handleMouseDown(x, y, ArcballManip::Right, mod);
      } else if (rightDown && g_rightDown) {
        manip.handleMouseMove(x, y, ArcballManip::Right, mod);
      } else if (!rightDown && g_rightDown) {
        manip.handleMouseUp(x, y, ArcballManip::Right, mod);
      }

      // middle
      if (middleDown && !g_middleDown) {
        manip.handleMouseDown(x, y, ArcballManip::Middle, mod);
      } else if (middleDown && g_middleDown) {
        manip.handleMouseMove(x, y, ArcballManip::Middle, mod);
      } else if (!middleDown && g_middleDown) {
        manip.handleMouseUp(x, y, ArcballManip::Middle, mod);
      }

      g_leftDown = leftDown;
      g_rightDown = rightDown;
      g_middleDown = middleDown;
    }

    bool hasCameraChanged() const
    {
      return cameraChanged;
    }

   private:
    ArcballManip manip;
    bool cameraChanged = false;
  };
} // namespace util
