/******************************************************************************
 * Intercept plugin for OpenCPN -- moving-target intercept solve.
 *
 * Copyright (C) 2026 momo
 * License: GPLv3+  (see COPYING)
 ******************************************************************************/

#include "intercept_solve.h"

#include <algorithm>
#include <cmath>

#include "intercept.h"

namespace {

// No target this plugin models moves faster: a hand-entered drift is
// clamped to 20 kt, and leeway + current stay far below that.
constexpr double kMaxTargetSpeedKt = 20.0;
// ComputeAgedDatum's own elapsed cap: past it the trajectory stops advancing.
constexpr double kCapHours = 30.0 * 24.0;
// Near the intercept the safe step shrinks towards zero; don't step less.
// An intercept window shorter than this could be stepped over.
constexpr double kMinStepHours = 1.0 / 60.0;
constexpr double kTimeTolHours = 1.0 / 3600.0;  // 1 second.
// Each evaluation ages the datum once; bounds the cost of a hopeless case.
constexpr int kMaxEvaluations = 2000;

}  // namespace

MovingInterceptResult SolveMovingIntercept(
    double own_lat, double own_lon, double own_sog_kt,
    const std::function<GeoPoint(double)>& target_at) {
  MovingInterceptResult result;
  if (!(own_sog_kt > 0.0)) return result;  // No speed -> no intercept.

  // How far own-ship is still short of where the target is at t, having run
  // at full speed until then. The intercept is its first zero.
  int evaluations = 0;
  auto shortfall_nm = [&](double t) {
    ++evaluations;
    const GeoPoint p = target_at(t);
    return CourseToSteer(own_lat, own_lon, p.lat, p.lon, own_sog_kt)
               .distance_nm -
           own_sog_kt * t;
  };

  const double max_closing_kt = own_sog_kt + kMaxTargetSpeedKt;
  double lo = 0.0;
  double hi = 0.0;
  double shortfall = shortfall_nm(lo);
  if (shortfall > 0.0) {
    // March: shortfall(lo) > 0 throughout.
    for (;;) {
      hi = lo + std::max(shortfall / max_closing_kt, kMinStepHours);
      if (hi > kCapHours || evaluations >= kMaxEvaluations) return result;
      const double s = shortfall_nm(hi);
      if (s <= 0.0) break;
      lo = hi;
      shortfall = s;
    }
    // Bisect: shortfall(lo) > 0 >= shortfall(hi).
    while (hi - lo > kTimeTolHours && evaluations < kMaxEvaluations) {
      const double mid = 0.5 * (lo + hi);
      if (shortfall_nm(mid) > 0.0) {
        lo = mid;
      } else {
        hi = mid;
      }
    }
  }

  // hi is when own-ship can be at the target's position: steer there.
  const GeoPoint meet = target_at(hi);
  const InterceptResult leg =
      CourseToSteer(own_lat, own_lon, meet.lat, meet.lon, own_sog_kt);
  result.converged = true;
  result.lat = meet.lat;
  result.lon = meet.lon;
  result.bearing_deg = leg.bearing_deg;
  result.distance_nm = leg.distance_nm;
  result.time_hours = hi;
  return result;
}
