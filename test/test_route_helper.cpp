/******************************************************************************
 * Intercept plugin for OpenCPN -- standalone test for waypoint-list
 * building.
 *
 * Builds without the OpenCPN plugin SDK or wxWidgets: route_helper.h is
 * plain C++ over doubles. See CMakeLists.txt.
 *
 * Copyright (C) 2026 momo
 * License: GPLv3+  (see COPYING)
 ******************************************************************************/

#include "route_helper.h"

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
  // (1) Own-ship north of the equator, datum south and east of it --
  // exercises negative/positive lat and lon together.
  {
    std::vector<GeoPoint> wps =
        BuildInterceptWaypoints(36.123456, -5.987654, -12.5, 45.75);
    Check(wps.size() == 2, "exactly two waypoints");
    if (wps.size() == 2) {
      Check(wps[0].lat == 36.123456 && wps[0].lon == -5.987654,
            "first waypoint is own-ship position");
      Check(wps[1].lat == -12.5 && wps[1].lon == 45.75,
            "second waypoint is the datum");
    }
  }

  // (2) Own-ship and datum at the same point (zero-length line): still two
  // distinct list entries, both with equal coordinates.
  {
    std::vector<GeoPoint> wps =
        BuildInterceptWaypoints(0.0, 0.0, 0.0, 0.0);
    Check(wps.size() == 2, "same-position: still two waypoints");
    if (wps.size() == 2) {
      Check(wps[0].lat == 0.0 && wps[0].lon == 0.0,
            "same-position: first waypoint at origin");
      Check(wps[1].lat == 0.0 && wps[1].lon == 0.0,
            "same-position: second waypoint at origin");
    }
  }

  // (3) A non-finite coordinate is replaced with 0, keeping the two-point
  // contract so callers can index [0]/[1] unconditionally.
  {
    const double nan = std::nan("");
    std::vector<GeoPoint> wps =
        BuildInterceptWaypoints(nan, 5.0, 10.0, HUGE_VAL);
    Check(wps.size() == 2, "non-finite: still two waypoints");
    if (wps.size() == 2) {
      Check(wps[0].lat == 0.0 && wps[0].lon == 5.0,
            "non-finite: NaN lat -> 0, finite lon kept");
      Check(wps[1].lat == 10.0 && wps[1].lon == 0.0,
            "non-finite: Inf lon -> 0, finite lat kept");
    }
  }

  if (g_failures > 0) {
    std::fprintf(stderr, "%d check(s) failed\n", g_failures);
    return 1;
  }
  std::printf("All checks passed.\n");
  return 0;
}
