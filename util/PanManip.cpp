#include "PanManip.h"

using namespace anari::math;

namespace util {

  PanManip::PanManip(Camera &cam) : camera(cam) {}

  void PanManip::resize(int width, int height)
  {
    size = int2(width, height);
  }

  void PanManip::handleMouseDown(int x, int y, MouseButton button, ModifierKey mod)
  {
    if (!isDragging) {
      lastPos = int2(x, y);
      isDragging = true;
    }
  }

  void PanManip::handleMouseUp(int x, int y, MouseButton button, ModifierKey mod)
  {
    isDragging = false;
  }

  void PanManip::handleMouseMove(int x, int y, MouseButton button, ModifierKey mod)
  {
    if (isDragging) {
      float w(size.x);
      float h(size.y);
      float dx =  (float)(lastPos.x-x)/w;
      float dy = -(float)(lastPos.y-y)/h;
      float s = 2.f * camera.getDistance();
      float3 W = normalize(camera.getEye() - camera.getCenter());
      float3 V = camera.getUp();
      float3 U = cross(V,W);
      float3 d = (dx * s) * U + (dy * s) * V;

      camera.lookAt(camera.getEye() + d, camera.getCenter() + d, camera.getUp());

      lastPos = int2(x, y);
    }
  }

} // namespace util
