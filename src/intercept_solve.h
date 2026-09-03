/******************************************************************************
 * Intercept plugin for OpenCPN -- moving-target intercept solve.
 *
 * Copyright (C) 2026 momo
 * License: GPLv3+  (see COPYING)
 ******************************************************************************/

#ifndef INTERCEPT_PI_INTERCEPT_SOLVE_H__
#define INTERCEPT_PI_INTERCEPT_SOLVE_H__

#include <functional>

#include "route_helper.h"  // GeoPoint

/**
 * Outcome of SolveMovingIntercept. `converged` is false when own-ship
 * cannot reach the target -- speed is zero/unknown, or the target drifts
 * away faster than own-ship closes -- in which case the other fields are
 * unset and the caller should fall back to steering at the target's
 * present position.
 */
struct MovingInterceptResult {
  bool converged = false;
  double lat = 0.0;           // Where own-ship and the target meet.
  double lon = 0.0;
  double bearing_deg = 0.0;   // Constant course to steer to get there.
  double distance_nm = 0.0;   // Length of own-ship's track to the meeting point.
  double time_hours = 0.0;    // Transit time == ETA.
};

/**
 * Solves the classic moving-target intercept: own-ship leaves (own_lat,
 * own_lon) now at own_sog_kt and steers a straight course; the target
 * moves along the trajectory given by target_at(hours_from_now) (hours
 * >= 0, hours == 0 is the target's present position). Returns the point,
 * course and time at which they coincide.
 *
 * Method: fixed-point iteration on the transit time -- t <- range(own,
 * target_at(t)) / own_sog_kt -- which contracts whenever own-ship is
 * faster than the target's along-track speed. Great-circle range/bearing
 * come from CourseToSteer(). Gives up (converged = false) if own_sog_kt
 * <= 0, if the implied transit time runs past ~30 days (target is
 * outrunning own-ship), or if it has not settled within a fixed iteration
 * budget.
 */
MovingInterceptResult SolveMovingIntercept(
    double own_lat, double own_lon, double own_sog_kt,
    const std::function<GeoPoint(double)>& target_at);

#endif  // INTERCEPT_PI_INTERCEPT_SOLVE_H__
