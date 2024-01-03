
#pragma once

// anari
#include "anari/anari_cpp/ext/linalg.h"

namespace anari::math {

  struct box3
  {
    box3() = default;
    box3(float3 lo, float3 up) : lower(lo), upper(up) {}

    float3 lower, upper;
  };
} // namespace anari::math
