
#pragma once

// anari
#include "anari/anari_cpp/ext/linalg.h"

namespace anari::math {

  struct box3
  {
    box3() = default;
    box3(float3 lo, float3 up) : lower(lo), upper(up) {}

    float3 lower, upper;

    float3 size() const
    { return upper-lower; }

    float3 center() const
    { return lower+size()*float3(0.5f); }

    float volume() const
    { return size().x*size().y*size().z; }

    box3& extend(float3 p) {
      lower.x = fminf(lower.x, p.x);
      lower.y = fminf(lower.y, p.y);
      lower.z = fminf(lower.z, p.z);

      upper.x = fmaxf(upper.x, p.x);
      upper.y = fmaxf(upper.y, p.y);
      upper.z = fmaxf(upper.z, p.z);

      return *this;
    }

    box3& extend(const box3 &b) {
      return extend(b.lower).extend(b.upper);
    }
  };

  struct box3i
  {
    box3i() = default;
    box3i(int3 lo, int3 up) : lower(lo), upper(up) {}

    int3 lower, upper;

    int3 size() const
    { return upper-lower; }

    int3 center() const
    { return lower+size()/int3(2); }

    long long volume() const
    { return size().x*(long long)size().y*size().z; }

    box3i& extend(int3 p) {
      lower.x = min(lower.x, p.x);
      lower.y = min(lower.y, p.y);
      lower.z = min(lower.z, p.z);

      upper.x = max(upper.x, p.x);
      upper.y = max(upper.y, p.y);
      upper.z = max(upper.z, p.z);

      return *this;
    }

    box3i& extend(const box3i &b) {
      return extend(b.lower).extend(b.upper);
    }
  };
} // namespace anari::math
