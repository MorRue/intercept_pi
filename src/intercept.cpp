/******************************************************************************
 * Intercept plugin for OpenCPN -- course to steer.
 *
 * Copyright (C) 2026 momo
 * License: GPLv3+  (see COPYING)
 ******************************************************************************/

#include "portability.h"  // must precede <cmath>: M_PI on MSVC

#include "intercept.h"

namespace {

constexpr double kEarthRadiusNm = 3440.065;

double ToRad(double deg) { return deg * M_PI / 180.0; }
double ToDeg(double rad) { return rad * 180.0 / M_PI; }

double NormalizeDegrees(double deg) {
  double d = std::fmod(deg, 360.0);
  if (d < 0.0) d += 360.0;
  return d;
}

}  // namespace

InterceptResult CourseToSteer(double own_lat, double own_lon,
                               double target_lat, double target_lon,
                               double own_sog_kt) {
  InterceptResult result;

  const double phi1 = ToRad(own_lat);
  const double phi2 = ToRad(target_lat);
  const double delta_phi = ToRad(target_lat - own_lat);
  const double delta_lambda = ToRad(target_lon - own_lon);

  // Haversine great-circle distance.
  const double a = std::sin(delta_phi / 2.0) * std::sin(delta_phi / 2.0) +
                    std::cos(phi1) * std::cos(phi2) *
                        std::sin(delta_lambda / 2.0) *
                        std::sin(delta_lambda / 2.0);
  const double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
  result.distance_nm = kEarthRadiusNm * c;

  // Initial great-circle bearing (atan2 formula).
  const double y = std::sin(delta_lambda) * std::cos(phi2);
  const double x = std::cos(phi1) * std::sin(phi2) -
                    std::sin(phi1) * std::cos(phi2) * std::cos(delta_lambda);
  result.bearing_deg = NormalizeDegrees(ToDeg(std::atan2(y, x)));

  if (own_sog_kt > 0.0) {
    result.eta = result.distance_nm / own_sog_kt;
  }

  return result;
}
