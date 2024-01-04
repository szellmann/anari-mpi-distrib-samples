
#pragma once

// glfw
#include <GLFW/glfw3.h>
// anari
#include "anari/anari_cpp/ext/linalg.h"
// ours
#include "ArcballManip.h"
#include "PanManip.h"

namespace util {
  struct ArcballCamera : Camera
  {
    using int2 = anari::math::int2;

    ArcballCamera(Camera::box3 worldBounds, int2 winSize)
      : rotateManip((Camera &)*this),
        panManip((Camera &)*this)
    {
      viewAll(worldBounds);
      rotateManip.resize(winSize.x, winSize.y);
      panManip.resize(winSize.x, winSize.y);
    }

    void updateWindowSize(int2 winSize)
    {
      rotateManip.resize(winSize.x, winSize.y);
      panManip.resize(winSize.x, winSize.y);
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

      ArcballManip::ModifierKey mod = ArcballManip::ModifierKey::None; // TODO!

      // left
      if (leftDown && !g_leftDown) {
        rotateManip.handleMouseDown(x, y, Manip::MouseButton::Left, mod);
      } else if (leftDown && g_leftDown) {
        rotateManip.handleMouseMove(x, y, Manip::MouseButton::Left, mod);
      } else if (!leftDown && g_leftDown) {
        rotateManip.handleMouseUp(x, y, Manip::MouseButton::Left, mod);
      }

      // right
      if (rightDown && !g_rightDown) {
        rotateManip.handleMouseDown(x, y, Manip::MouseButton::Right, mod);
      } else if (rightDown && g_rightDown) {
        rotateManip.handleMouseMove(x, y, Manip::MouseButton::Right, mod);
      } else if (!rightDown && g_rightDown) {
        rotateManip.handleMouseUp(x, y, Manip::MouseButton::Right, mod);
      }

      // middle
      if (middleDown && !g_middleDown) {
        panManip.handleMouseDown(x, y, Manip::MouseButton::Middle, mod);
      } else if (middleDown && g_middleDown) {
        panManip.handleMouseMove(x, y, Manip::MouseButton::Middle, mod);
      } else if (!middleDown && g_middleDown) {
        panManip.handleMouseUp(x, y, Manip::MouseButton::Middle, mod);
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
    ArcballManip rotateManip;
    PanManip panManip;
    bool cameraChanged = false;
  };
} // namespace util
