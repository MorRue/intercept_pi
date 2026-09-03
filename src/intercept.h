/******************************************************************************
 * Intercept plugin for OpenCPN -- course to steer.
 *
 * Copyright (C) 2026 momo
 * License: GPLv3+  (see COPYING)
 ******************************************************************************/

#ifndef INTERCEPT_PI_INTERCEPT_H__
#define INTERCEPT_PI_INTERCEPT_H__

#include <optional>

/**
 * Result of CourseToSteer: the great-circle course from own-ship to the
 * target, and how long it takes to get there at the given speed.
 */
struct InterceptResult {
  double bearing_deg = 0.0;
  double distance_nm = 0.0;
  // Hours to close distance_nm at own_sog_kt. Absent when own_sog_kt <= 0
  // (SOG unknown or the vessel is stationary) rather than a bogus infinite
  // or divide-by-zero value.
  std::optional<double> eta;
};

/**
 * Great-circle initial bearing and distance from own-ship to a fixed
 * target position -- the static, no-lead form. Correct as the course to
 * steer only when the target is not moving further; SolveMovingIntercept()
 * (intercept_solve.h) builds the moving-target lead on top of it by calling
 * this once per iteration against the target's extrapolated position.
 * Bearing is via the atan2 two-argument formula; distance is haversine,
 * both on a spherical-earth approximation, in nautical miles.
 *
 * Coordinates are expected in degrees; a non-finite input (NaN/Inf) yields
 * a zeroed result (distance 0, bearing 0, no ETA) rather than propagating.
 * Finite but out-of-range degrees are left to the periodic trig -- that is
 * a caller bug, not something this function masks.
 */
InterceptResult CourseToSteer(double own_lat, double own_lon,
                               double target_lat, double target_lon,
                               double own_sog_kt);

#endif  // INTERCEPT_PI_INTERCEPT_H__
