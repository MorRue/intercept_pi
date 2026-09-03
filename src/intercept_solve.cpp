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

MovingInterceptResult SolveMovingIntercept(
    double own_lat, double own_lon, double own_sog_kt,
    const std::function<GeoPoint(double)>& target_at) {
  MovingInterceptResult result;
  if (!(own_sog_kt > 0.0)) return result;  // No speed -> no intercept.

  // Budget and tolerances. kCapHours matches ComputeAgedDatum's own 30-day
  // elapsed cap: past that the trajectory stops advancing, so if the transit
  // time reaches it the target is simply outrunning own-ship.
  constexpr int kMaxIters = 100;
  constexpr double kAbsTolHours = 1.0 / 3600.0;  // 1 second.
  constexpr double kRelTol = 0.005;              // 0.5% of the transit time.
  constexpr double kCapHours = 30.0 * 24.0;

  double t = 0.0;
  for (int i = 0; i < kMaxIters; ++i) {
    const GeoPoint target = target_at(t < 0.0 ? 0.0 : t);
    const InterceptResult leg =
        CourseToSteer(own_lat, own_lon, target.lat, target.lon, own_sog_kt);
    const double t_next = leg.distance_nm / own_sog_kt;

    const double tol = std::max(kAbsTolHours, kRelTol * t_next);
    if (std::fabs(t_next - t) <= tol) {
      result.converged = true;
      result.lat = target.lat;
      result.lon = target.lon;
      result.bearing_deg = leg.bearing_deg;
      result.distance_nm = leg.distance_nm;
      result.time_hours = t_next;
      return result;
    }
    if (t_next > kCapHours) return result;  // Target outpaces own-ship.
    t = t_next;
  }
  return result;  // Did not settle within the budget.
}
