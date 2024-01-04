#include "ArcballManip.h"

using namespace anari::math;

namespace util {

  // ======================================================
  // Arcball implementation
  // ======================================================

  float3 Arcball::project(int x, int y, int2 winSize)
  {
    float3 res;
    res.x =  (x - 0.5f * winSize.x) / (radius * 0.5f * winSize.x);
    res.y = -(y - 0.5f * winSize.y) / (radius * 0.5f * winSize.y);
    float d = res.x*res.x+res.y*res.y;
    if (d > 1.f) {
      float len = sqrtf(d);
      res.x /= len;
      res.y /= len;
      res.z = 0.f;
    } else {
      res.z = sqrtf(1.f - d);
    }
    return res;
  }



  // ======================================================
  // Arcball manip implementation
  // ======================================================

  ArcballManip::ArcballManip(Camera &cam) : camera(cam) {}

  void ArcballManip::resize(int width, int height)
  {
    size = int2(width, height);
  }

  void ArcballManip::handleMouseDown(int x, int y, MouseButton button, ModifierKey mod)
  {
    if (!isDragging) {
      downPos = ball.project(x, y, size);
      downRotation = rotation;
      isDragging = true;
    }
  }

  void ArcballManip::handleMouseUp(int x, int y, MouseButton button, ModifierKey mod)
  {
    isDragging = false;
  }

  void ArcballManip::handleMouseMove(int x, int y, MouseButton button, ModifierKey mod)
  {
    if (isDragging) {
      float3 currPos = ball.project(x, y, size);
      float3 from = normalize(downPos);
      float3 to = normalize(currPos);
      rotation = qmul(Quat(cross(from, to), dot(from, to)), downRotation);

      mat3 rotationMat = qmat(qconj(rotation));

      float3 eye(0, 0, camera.getDistance());
      eye = mul(rotationMat, eye);
      eye += camera.getCenter();

      float3 up = rotationMat[1];

      camera.lookAt(eye, camera.getCenter(), up);
    }
  }

} // namespace util
