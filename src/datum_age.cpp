/******************************************************************************
 * Intercept plugin for OpenCPN -- datum ageing.
 *
 * Copyright (C) 2026 momo
 * License: GPLv3+  (see COPYING)
 ******************************************************************************/

#include "datum_age.h"

#include "grib_reader.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr double kEarthRadiusNm = 3440.065;
constexpr long kStepSeconds = 1800;  // 30 minutes.
// Bounds the number of integration steps for a very old report -- ageing
// runs synchronously when the case is recalculated, so a months-old case must not
// stall the UI thread. Beyond this many steps, the step size grows past
// kStepSeconds instead of the step count growing past kMaxSteps.
constexpr long kMaxSteps = 2000;

double NormalizeDegrees(double deg) {
  double d = std::fmod(deg, 360.0);
  if (d < 0.0) d += 360.0;
  return d;
}

double ToRad(double deg) { return deg * M_PI / 180.0; }
double ToDeg(double rad) { return rad * 180.0 / M_PI; }

/**
 * Rhumb-line direct problem: the position distance_nm along bearing_deg
 * (true) from (lat, lon). A rhumb line is adequate for the short, steady
 * -bearing steps used to integrate drift; a great-circle solution would be
 * needless precision here.
 */
void AdvancePosition(double lat, double lon, double bearing_deg,
                      double distance_nm, double* out_lat, double* out_lon) {
  double delta = distance_nm / kEarthRadiusNm;
  double theta = ToRad(bearing_deg);
  double phi1 = ToRad(lat);

  double delta_phi = delta * std::cos(theta);
  double phi2 = phi1 + delta_phi;
  // Guard the isometric-latitude singularity at the poles.
  phi2 = std::max(-M_PI / 2.0 + 1e-9, std::min(M_PI / 2.0 - 1e-9, phi2));

  double delta_psi = std::log(std::tan(M_PI / 4.0 + phi2 / 2.0) /
                               std::tan(M_PI / 4.0 + phi1 / 2.0));
  double q = (std::fabs(delta_psi) > 1e-12) ? delta_phi / delta_psi
                                             : std::cos(phi1);

  double delta_lambda = delta * std::sin(theta) / q;
  double lambda2 = ToRad(lon) + delta_lambda;

  *out_lat = ToDeg(phi2);
  *out_lon = NormalizeDegrees(ToDeg(lambda2) + 180.0) - 180.0;
}

}  // namespace

void CombineVectors(double speed1_kt, double dir1_deg, double speed2_kt,
                     double dir2_deg, double* out_speed_kt,
                     double* out_dir_deg) {
  double x = speed1_kt * std::sin(ToRad(dir1_deg)) +
             speed2_kt * std::sin(ToRad(dir2_deg));
  double y = speed1_kt * std::cos(ToRad(dir1_deg)) +
             speed2_kt * std::cos(ToRad(dir2_deg));
  // Equal-and-opposite vectors (e.g. current cancelling leeway) can leave x
  // and y exactly zero, where atan2's result at the origin is a platform
  // convention rather than a meaningful direction.
  if (x == 0.0 && y == 0.0) {
    *out_speed_kt = 0.0;
    *out_dir_deg = 0.0;
    return;
  }
  *out_speed_kt = std::sqrt(x * x + y * y);
  *out_dir_deg = NormalizeDegrees(ToDeg(std::atan2(x, y)));
}

LeewayCoefficients LookupLeewayCoefficients(const wxString& craft_type) {
  if (craft_type.Lower().Contains("wooden")) {
    return LeewayCoefficients{0.04, 0.0};
  }
  // 0.036 is mid liferaft range per IAMSAR Vol II Fig N-2 / Allen & Plourde
  // 1999 -- see docs/LEEWAY_NEEDS_VERIFICATION.md. (0.36 was ~10x too large.)
  return LeewayCoefficients{0.036, 0.0};  // Rubber-hulled, and the default.
}

AgedDatum ComputeAgedDatum(double reported_lat, double reported_lon,
                           const wxDateTime& time_of_report,
                           const wxDateTime& now, const wxString& craft_type,
                           const GribReader* grib,
                           const ManualSetAndDrift& manual) {
  AgedDatum result;
  result.lat = reported_lat;
  result.lon = reported_lon;
  result.elapsed = wxTimeSpan();

  if (!time_of_report.IsValid() || !now.IsValid() || now <= time_of_report) {
    return result;
  }

  result.elapsed = now - time_of_report;
  long total_seconds = result.elapsed.GetSeconds().ToLong();

  if (!grib && !manual.available) {
    return result;  // Zero drift: aged datum equals reported position.
  }

  const LeewayCoefficients leeway = LookupLeewayCoefficients(craft_type);

  // Sanitise operator-entered set & drift once, before integrating: a
  // non-finite value (NaN from a bad text-field parse) drifts nothing
  // rather than propagating NaN through every subsequent step, a negative
  // drift_kt is clamped to 0 rather than driving the datum backwards, and
  // an out-of-range set_deg is normalised into a true bearing.
  double manual_drift_kt = 0.0;
  double manual_set_deg = 0.0;
  if (manual.available && std::isfinite(manual.drift_kt) &&
      std::isfinite(manual.set_deg)) {
    manual_drift_kt = std::max(0.0, manual.drift_kt);
    manual_set_deg = NormalizeDegrees(manual.set_deg);
  }

  long step_seconds = kStepSeconds;
  if (total_seconds / step_seconds > kMaxSteps) {
    step_seconds = (total_seconds + kMaxSteps - 1) / kMaxSteps;
  }

  double lat = reported_lat;
  double lon = reported_lon;
  wxDateTime step_time = time_of_report;
  long remaining_seconds = total_seconds;

  while (remaining_seconds > 0) {
    long this_step_seconds = std::min(step_seconds, remaining_seconds);

    double drift_speed_kt = 0.0;
    double drift_dir_deg = 0.0;

    if (grib) {
      EnvSample wind = grib->LookupWind(lat, lon, step_time);
      EnvSample current = grib->LookupCurrent(lat, lon, step_time);

      double leeway_speed_kt = 0.0;
      double leeway_dir_deg = 0.0;
      if (wind.available) {
        leeway_speed_kt =
            std::max(0.0, leeway.speed_pct_of_wind * wind.speed_kt +
                              leeway.speed_constant_kt);
        leeway_dir_deg = NormalizeDegrees(wind.direction_deg + 180.0);
      }

      if (current.available && wind.available) {
        CombineVectors(current.speed_kt, current.direction_deg,
                       leeway_speed_kt, leeway_dir_deg, &drift_speed_kt,
                       &drift_dir_deg);
      } else if (current.available) {
        drift_speed_kt = current.speed_kt;
        drift_dir_deg = current.direction_deg;
      } else if (wind.available) {
        drift_speed_kt = leeway_speed_kt;
        drift_dir_deg = leeway_dir_deg;
      }
    } else if (manual.available) {
      drift_speed_kt = manual_drift_kt;
      drift_dir_deg = manual_set_deg;
    }

    if (drift_speed_kt > 0.0) {
      double distance_nm = drift_speed_kt * (this_step_seconds / 3600.0);
      double new_lat, new_lon;
      AdvancePosition(lat, lon, drift_dir_deg, distance_nm, &new_lat,
                       &new_lon);
      lat = new_lat;
      lon = new_lon;
    }

    step_time += wxTimeSpan::Seconds(this_step_seconds);
    remaining_seconds -= this_step_seconds;
  }

  result.lat = lat;
  result.lon = lon;
  return result;
}
