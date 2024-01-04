#include "ZoomManip.h"

using namespace anari::math;

namespace util {

  ZoomManip::ZoomManip(Camera &cam) : camera(cam) {}

  void ZoomManip::resize(int width, int height)
  {
    size = int2(width, height);
  }

  void ZoomManip::handleMouseDown(int x, int y, MouseButton button, ModifierKey mod)
  {
    if (!isDragging) {
      lastPos = int2(x, y);
      isDragging = true;
    }
  }

  void ZoomManip::handleMouseUp(int x, int y, MouseButton button, ModifierKey mod)
  {
    isDragging = false;
  }

  void ZoomManip::handleMouseMove(int x, int y, MouseButton button, ModifierKey mod)
  {
    if (isDragging) {
      float h(size.y);
      float dy = -(float)(lastPos.y-y)/h;
      float s = 2.f * camera.getDistance() * dy;
      float3 dir = normalize(camera.getEye() - camera.getCenter());

      camera.lookAt(camera.getEye() -dir * s, camera.getCenter(), camera.getUp());

      lastPos = int2(x, y);
    }
  }

} // namespace util
