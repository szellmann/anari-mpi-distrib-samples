
#pragma once

// anari
#include "anari/anari_cpp/ext/linalg.h"
// ours
#include "Camera.h"
#include "Manip.h"

namespace util {

  struct PanManip : public Manip
  {
    PanManip(Camera &cam);
    void resize(int width, int height);
    void handleMouseDown(
        int x, int y, MouseButton button, ModifierKey mod = ModifierKey::None);
    void handleMouseUp(
        int x, int y, MouseButton button, ModifierKey mod = ModifierKey::None);
    void handleMouseMove(
        int x, int y, MouseButton button, ModifierKey mod = ModifierKey::None);

   private:
    Camera &camera;

    anari::math::int2 size{10, 10};
    bool isDragging;

    anari::math::int2 lastPos;
  };

} // namespace util
