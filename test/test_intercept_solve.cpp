/******************************************************************************
 * Intercept plugin for OpenCPN -- standalone test for the moving-target solve.
 *
 * Builds without the OpenCPN plugin SDK or wxWidgets: SolveMovingIntercept()
 * is plain C++ over doubles and a std::function. See CMakeLists.txt.
 *
 * Copyright (C) 2026 momo
 * License: GPLv3+  (see COPYING)
 ******************************************************************************/

#include "intercept_solve.h"

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

// Near the equator, 1 NM is very close to 1/60 deg in both axes, so a
// planar intercept triangle can be laid out in lat (north) / lon (east)
// and checked against its closed-form solution.
constexpr double kNmPerDeg = 60.0;
double North(double nm) { return nm / kNmPerDeg; }
double East(double nm) { return nm / kNmPerDeg; }

}  // namespace

int main() {
  // (1) Classic intercept triangle. Own-ship at the origin, 5 kt. Target
  //     starts 6 NM north and drifts due east at 4 kt.
  //       |P(t)|^2 = 6^2 + (4t)^2 = (5t)^2  ->  9t^2 = 36  ->  t = 2 h.
  //     Meeting point: 6 NM north, 8 NM east; range 10 NM; bearing
  //     atan2(8, 6) = 53.13 deg.
  {
    auto target_at = [](double h) {
      return GeoPoint{North(6.0), East(4.0 * h)};
    };
    MovingInterceptResult r = SolveMovingIntercept(0.0, 0.0, 5.0, target_at);
    Check(r.converged, "triangle: converged");
    Check(std::fabs(r.time_hours - 2.0) < 0.01, "triangle: ETA ~2 h");
    Check(std::fabs(r.distance_nm - 10.0) < 0.05, "triangle: run ~10 NM");
    Check(std::fabs(r.bearing_deg - 53.13) < 0.5, "triangle: course ~53 deg");
    Check(std::fabs(r.lat - North(6.0)) < 1e-4, "triangle: meeting lat");
    Check(std::fabs(r.lon - East(8.0)) < 2e-3, "triangle: meeting lon ~8 NM E");
  }

  // (2) Stationary target reduces to a straight run: 6 NM north, 6 kt -> 1 h.
  {
    auto target_at = [](double) { return GeoPoint{North(6.0), 0.0}; };
    MovingInterceptResult r = SolveMovingIntercept(0.0, 0.0, 6.0, target_at);
    Check(r.converged, "stationary: converged");
    Check(std::fabs(r.time_hours - 1.0) < 1e-3, "stationary: ETA ~1 h");
    Check(std::fabs(r.bearing_deg) < 0.05 || std::fabs(r.bearing_deg - 360.0) < 0.05,
          "stationary: course due north");
  }

  // (3) Target outruns own-ship: starts 5 NM east, drifts east at 10 kt,
  //     own-ship only 5 kt. No intercept.
  {
    auto target_at = [](double h) {
      return GeoPoint{0.0, East(5.0 + 10.0 * h)};
    };
    MovingInterceptResult r = SolveMovingIntercept(0.0, 0.0, 5.0, target_at);
    Check(!r.converged, "target-outruns: no solution");
  }

  // (4) Speed not known -> no solution, immediately.
  {
    auto target_at = [](double) { return GeoPoint{North(6.0), 0.0}; };
    Check(!SolveMovingIntercept(0.0, 0.0, 0.0, target_at).converged,
          "zero speed: no solution");
    Check(!SolveMovingIntercept(0.0, 0.0, -3.0, target_at).converged,
          "negative speed: no solution");
  }

  // (5) The lead matters: with a crossing target the meeting point is well
  //     downrange of where the target is now.
  {
    auto target_at = [](double h) {
      return GeoPoint{North(6.0), East(4.0 * h)};
    };
    MovingInterceptResult r = SolveMovingIntercept(0.0, 0.0, 5.0, target_at);
    GeoPoint now = target_at(0.0);
    double lead_nm = std::hypot((r.lat - now.lat) * kNmPerDeg,
                                (r.lon - now.lon) * kNmPerDeg);
    Check(lead_nm > 7.0, "lead: meeting point is >7 NM from the present datum");
  }

  if (g_failures > 0) {
    std::fprintf(stderr, "%d check(s) failed\n", g_failures);
    return 1;
  }
  std::printf("All checks passed.\n");
  return 0;
}
