/******************************************************************************
 * Intercept plugin for OpenCPN -- route waypoint-list building.
 *
 * Copyright (C) 2026 momo
 * License: GPLv3+  (see COPYING)
 ******************************************************************************/

#include "route_helper.h"

std::vector<GeoPoint> BuildInterceptWaypoints(double own_lat, double own_lon,
                                               double datum_lat,
                                               double datum_lon) {
  return {GeoPoint{own_lat, own_lon}, GeoPoint{datum_lat, datum_lon}};
}
