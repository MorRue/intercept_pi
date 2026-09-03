/******************************************************************************
 * Intercept plugin for OpenCPN -- route waypoint-list building.
 *
 * Copyright (C) 2026 momo
 * License: GPLv3+  (see COPYING)
 ******************************************************************************/

#ifndef INTERCEPT_PI_ROUTE_HELPER_H__
#define INTERCEPT_PI_ROUTE_HELPER_H__

#include <vector>

// A plain lat/lon pair -- not a wxWidgets type. Kept independent of the
// OpenCPN plugin SDK's PlugIn_Waypoint (which needs a GUID, icon name, etc.
// and pulls in wxString) so the point list stays pure and testable; Next
// #2b in CLAUDE.md converts a list of these into a PlugIn_Route.
struct GeoPoint {
  double lat = 0.0;
  double lon = 0.0;
};

// Builds the two-point course line: own-ship's current position, then the
// point to steer at (the moving-target intercept, or the present datum when
// that didn't solve). Always returns exactly two points, in that order; a
// non-finite input coordinate is replaced with 0.
std::vector<GeoPoint> BuildInterceptWaypoints(double own_lat, double own_lon,
                                               double datum_lat,
                                               double datum_lon);

#endif  // INTERCEPT_PI_ROUTE_HELPER_H__
