/******************************************************************************
 * Intercept plugin for OpenCPN -- standalone test for course-to-steer.
 *
 * Builds without the OpenCPN plugin SDK or wxWidgets: CourseToSteer() is
 * plain C++ over doubles. See CMakeLists.txt.
 *
 * Copyright (C) 2026 momo
 * License: GPLv3+  (see COPYING)
 ******************************************************************************/

#include "intercept.h"

#include <cmath>
#include <cstdio>

namespace {

int g_failures = 0;

void Check(bool cond, const char* what) {
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", what);
    ++g_failures;
  } else {
    std::printf("ok:   %s\n", what);
  }
}

}  // namespace

int main() {
  // (1) Due north: bearing 0, distance = 1 degree of latitude (~60 NM).
  {
    InterceptResult r = CourseToSteer(0.0, 0.0, 1.0, 0.0, 0.0);
    Check(std::fabs(r.bearing_deg - 0.0) < 1e-4, "due-north: bearing 0");
    Check(std::fabs(r.distance_nm - 60.040461) < 1e-3,
          "due-north: distance ~60.04 NM");
    Check(!r.eta.has_value(), "due-north: eta absent when SOG <= 0");
  }

  // (2) Due east on the equator: bearing 90, same distance as (1) since the
  // equator is itself a great circle.
  {
    InterceptResult r = CourseToSteer(0.0, 0.0, 0.0, 1.0, 0.0);
    Check(std::fabs(r.bearing_deg - 90.0) < 1e-4, "due-east: bearing 90");
    Check(std::fabs(r.distance_nm - 60.040461) < 1e-3,
          "due-east: distance ~60.04 NM");
  }

  // (3) Diagonal: initial great-circle bearing curves poleward of the
  // straight-line 45 deg, per the atan2 formula (hand-verified).
  {
    InterceptResult r = CourseToSteer(0.0, 0.0, 10.0, 10.0, 0.0);
    Check(std::fabs(r.bearing_deg - 44.561451) < 1e-3,
          "diagonal: bearing ~44.56 deg");
    Check(std::fabs(r.distance_nm - 846.933397) < 1e-2,
          "diagonal: distance ~846.93 NM");
  }

  // (4) Antimeridian crossing: 2 degrees of longitude apart the short way
  // (179 to -179), not 358 the long way.
  {
    InterceptResult r = CourseToSteer(0.0, 179.0, 0.0, -179.0, 0.0);
    Check(std::fabs(r.bearing_deg - 90.0) < 1e-4,
          "antimeridian: bearing 90 (shortest way, eastbound)");
    Check(std::fabs(r.distance_nm - 120.080921) < 1e-2,
          "antimeridian: distance ~120.08 NM (2 deg, not 358)");
  }

  // (5) ETA: present and correct when SOG > 0.
  {
    InterceptResult r = CourseToSteer(36.0, -5.0, 36.0, -4.0, 10.0);
    Check(r.eta.has_value(), "eta: present when SOG > 0");
    if (r.eta.has_value()) {
      Check(std::fabs(*r.eta - r.distance_nm / 10.0) < 1e-9,
            "eta: equals distance_nm / own_sog_kt");
      Check(std::fabs(*r.eta - 4.857354) < 1e-3, "eta: ~4.857 h");
    }
  }

  // (6) Same position: zero bearing, zero distance.
  {
    InterceptResult r = CourseToSteer(36.5, -5.5, 36.5, -5.5, 5.0);
    Check(r.distance_nm == 0.0, "same-position: zero distance");
    Check(r.eta.has_value() && *r.eta == 0.0,
          "same-position: eta present and zero when SOG > 0");
  }

  // (7) Negative SOG: eta absent, same as SOG == 0.
  {
    InterceptResult r = CourseToSteer(0.0, 0.0, 1.0, 0.0, -5.0);
    Check(!r.eta.has_value(), "negative SOG: eta absent");
  }

  // (8) Non-finite coordinate: zeroed result rather than NaN propagation.
  {
    const double nan = std::nan("");
    for (const auto& r :
         {CourseToSteer(nan, 0.0, 1.0, 0.0, 10.0),
          CourseToSteer(0.0, 0.0, nan, 0.0, 10.0),
          CourseToSteer(0.0, HUGE_VAL, 1.0, 0.0, 10.0)}) {
      Check(r.distance_nm == 0.0 && r.bearing_deg == 0.0 && !r.eta.has_value(),
            "non-finite coordinate: zeroed result");
    }
  }

  if (g_failures > 0) {
    std::fprintf(stderr, "%d check(s) failed\n", g_failures);
    return 1;
  }
  std::printf("All checks passed.\n");
  return 0;
}
