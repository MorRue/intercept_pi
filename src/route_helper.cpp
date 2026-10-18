/******************************************************************************
 * Intercept plugin for OpenCPN -- route waypoint-list building.
 *
 * Copyright (C) 2026 momo
 * License: GPLv3+  (see COPYING)
 ******************************************************************************/

#include "route_helper.h"

#include <cmath>

namespace {
// A non-finite coordinate reaching OpenCPN's route renderer can produce a
// degenerate or off-canvas line; collapse it to 0 so the caller draws a
// harmless zero-length segment instead.
double Finite(double v) { return std::isfinite(v) ? v : 0.0; }
}  // namespace

double RangeRingStepForUnits(double distance_nm, int ocpn_ring_units) {
  constexpr double kKmPerNm = 1.852;
  return ocpn_ring_units == 1 ? distance_nm * kKmPerNm : distance_nm;
}

std::vector<GeoPoint> BuildInterceptWaypoints(double own_lat, double own_lon,
                                               double datum_lat,
                                               double datum_lon) {
  return {GeoPoint{Finite(own_lat), Finite(own_lon)},
          GeoPoint{Finite(datum_lat), Finite(datum_lon)}};
}
