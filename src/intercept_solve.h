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
 * Outcome of SolveMovingIntercept. `converged` is false when there is no
 * intercept -- own-ship speed is zero/unknown, or own-ship can never reach
 * the target (e.g. a faster target drifting away) -- in which case the other
 * fields are unset.
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
 * course and time of the EARLIEST meeting -- also when the target is the
 * faster of the two but heading toward or across own-ship's reach.
 *
 * Method: the intercept is the first t at which the shortfall
 *   range(own, target_at(t)) - own_sog_kt * t
 * reaches zero. Since that shortfall can fall no faster than own-ship speed
 * plus the target's (taken as at most 20 kt, the plugin's plausibility cap
 * on drift), stepping forward by shortfall / (own_sog_kt + 20) can't jump
 * over that first zero; once a step reaches it, bisection pins it to about a
 * second. Great-circle range/bearing come from CourseToSteer(). Gives up
 * (converged = false) if own_sog_kt <= 0, past ~30 days (ComputeAgedDatum's
 * cap), or after a fixed evaluation budget.
 */
MovingInterceptResult SolveMovingIntercept(
    double own_lat, double own_lon, double own_sog_kt,
    const std::function<GeoPoint(double)>& target_at);

#endif  // INTERCEPT_PI_INTERCEPT_SOLVE_H__
