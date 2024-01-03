
#pragma once

// anari
#include "anari/anari_cpp/ext/linalg.h"
// ours
#include "Camera.h"

namespace util {

  struct Arcball
  {
    using int2 = anari::math::int2;
    using float3 = anari::math::float3;
  
    float3 project(int x, int y, int2 winSize);
  
    float radius{1.f};
  };
  
  struct ArcballManip
  {
    enum MouseButton { Left, Right, Middle, };
    enum ModifierKey { CTRL, ALT, Shift, None, };

    using int2 = anari::math::int2;
    using float3 = anari::math::float3;
    using Quat = anari::math::float4;

    ArcballManip(Camera &cam);
    void resize(int width, int height);
    void handleMouseDown(int x, int y, MouseButton button, ModifierKey mod = None);
    void handleMouseUp(int x, int y, MouseButton button, ModifierKey mod = None);
    void handleMouseMove(int x, int y, MouseButton button, ModifierKey mod = None);

   private:
    Camera &camera;

    int2 size{10, 10};
    bool isDragging{false};

    Arcball ball;
    float3 downPos{0.f, 0.f, 0.f};
    Quat rotation{0.f, 0.f, 0.f, 1.f};
    Quat downRotation{0.f, 0.f, 0.f, 1.f};
  };

} // namespace util
