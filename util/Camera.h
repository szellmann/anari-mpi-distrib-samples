
#pragma once

// stdlib
#include <cmath>
// anari
#include "anari/anari_cpp/ext/linalg.h"
// ours
#include "box3.h"

namespace util {
  struct Camera
  {
    using float3 = anari::math::float3;
    using mat4 = anari::math::mat4;
    using box3 = anari::math::box3;
    constexpr static float deg2rad = 1.74532925199432957692369076849e-02f;

    void lookAt(float3 eye, float3 center, float3 up)
    {
      this->eye = eye;
      this->center = center;
      this->up = up;
      distance = length(eye - center);

      float3 f = normalize(eye - center);
      float3 s = normalize(cross(up, f));
      float3 u = cross(f, s);

      view = mat4({s.x, u.x, f.x, 0.f},
                  {s.y, u.y, f.y, 0.f},
                  {s.z, u.z, f.z, 0.f},
                  {-dot(eye, s), -dot(eye, u), -dot(eye, f), 1.f});
    }

    void perspective(float fovy, float aspect, float znear, float zfar)
    {
      this->fovy = fovy;
      this->aspect = aspect;
      this->znear = znear;
      this->zfar = zfar;

      float f = 1.f/tanf(fovy * 0.5f);

      proj = mat4({f / aspect, 0.f, 0.f, 0.f},
                  {0.f, f, 0.f, 0.f},
                  {0.f, 0.f, (zfar+znear)/(znear-zfar), (2.f*zfar*znear)/(znear-zfar)},
                  {0.f, 0.f, -1.f, 0.f});
    }

    void viewAll(box3 bounds, float3 up = {0, 1, 0})
    {
      float3 boundsCenter = (bounds.lower + bounds.upper) * 0.5f;

      float diag = length(bounds.upper - bounds.lower);
      float r = diag * 0.5f;

      float3 eye = boundsCenter + float3(0, 0, r + r / atanf(fovy));

      lookAt(eye, boundsCenter, up);
    }

    float3 getEye() const { return eye; }
    float3 getCenter() const { return center; }
    float3 getUp() const { return up; }
    float getDistance() const { return distance; }
    mat4 getView() const { return view; }
    mat4 getProj() const { return proj; }

   private:
    float3 eye{0.f, 0.f, 0.f};
    float3 center{0.f, 0.f, -1.f};
    float3 up{0.f, 1.f, 0.f};
    float distance = 1.f;

    float fovy = 60.f*deg2rad;
    float aspect = 1.f;
    float znear = 1e-3f;
    float zfar = 1e3f;

    mat4 view, proj;
  };

} // namespace util
