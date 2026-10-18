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

// IAMSAR default probable errors (Vol II App. Q uses 0.3 kt for each) for a
// drift component the data covers, and a wider guess for one it doesn't.
// Unverified defaults -- see docs/LEEWAY_NEEDS_VERIFICATION.md.
constexpr double kKnownComponentErrorKt = 0.3;
constexpr double kUnknownComponentErrorKt = 0.5;

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

LeewayCoefficients LookupLeewayCoefficients(CraftType craft) {
  switch (craft) {
    case CraftType::kWooden:
      return LeewayCoefficients{0.04, 0.0, 50.0};
    case CraftType::kRubber:
    case CraftType::kUnknown:
      break;
  }
  // 0.036 is mid-range for a light rubber hull per the leeway curves in
  // IAMSAR Vol II Fig N-2 / Allen & Plourde 1999 -- see
  // docs/LEEWAY_NEEDS_VERIFICATION.md. (0.36 was ~10x too large.)
  return LeewayCoefficients{0.036, 0.0, 30.0};  // Rubber, and the default.
}

AgedDatum ComputeAgedDatum(double reported_lat, double reported_lon,
                           const wxDateTime& time_of_report,
                           const wxDateTime& now, CraftType craft,
                           const GribReader* grib,
                           const ManualSetAndDrift& manual,
                           bool include_leeway) {
  AgedDatum result;
  result.lat = reported_lat;
  result.lon = reported_lon;
  result.elapsed = wxTimeSpan();

  if (!time_of_report.IsValid() || !now.IsValid() || now <= time_of_report) {
    return result;
  }

  result.elapsed = now - time_of_report;
  long total_seconds = result.elapsed.GetSeconds().ToLong();

  // Guard against a nonsensical report time (some wxTimePickerCtrl builds
  // return a 1970 date): a useful datum is at most days old, and the
  // straight-line drift model is meaningless past that anyway.
  constexpr long kMaxElapsedSeconds = 30L * 24 * 3600;
  if (total_seconds > kMaxElapsedSeconds) {
    total_seconds = kMaxElapsedSeconds;
    result.elapsed = wxTimeSpan::Seconds(kMaxElapsedSeconds);
  }

  if (!grib && !manual.available) {
    return result;  // Zero drift: aged datum equals reported position.
  }
  result.source = grib ? DriftSource::kGrib : DriftSource::kManual;
  result.leeway_applied = grib != nullptr && include_leeway;

  const LeewayCoefficients leeway = LookupLeewayCoefficients(craft);

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
  // Cleared by the first step that lacks wind / current; with no step run
  // at all nothing was sampled, so nothing counts as known.
  result.wind_every_step = grib != nullptr && remaining_seconds > 0;
  result.current_every_step = result.wind_every_step;
  // Net leeway displacement (NM east, NM north).
  double leeway_east_nm = 0.0;
  double leeway_north_nm = 0.0;

  while (remaining_seconds > 0) {
    long this_step_seconds = std::min(step_seconds, remaining_seconds);

    double drift_speed_kt = 0.0;
    double drift_dir_deg = 0.0;

    if (grib) {
      EnvSample wind = grib->LookupWind(lat, lon, step_time);
      EnvSample current = grib->LookupCurrent(lat, lon, step_time);
      if (!wind.available) result.wind_every_step = false;
      if (!current.available) result.current_every_step = false;

      double leeway_speed_kt = 0.0;
      double leeway_dir_deg = 0.0;
      const bool leeway = wind.available && result.leeway_applied;
      if (leeway) {
        leeway_speed_kt =
            std::max(0.0, leeway.speed_pct_of_wind * wind.speed_kt +
                              leeway.speed_constant_kt);
        leeway_dir_deg = NormalizeDegrees(wind.direction_deg + 180.0);
        const double leeway_step_nm =
            leeway_speed_kt * (this_step_seconds / 3600.0);
        leeway_east_nm += leeway_step_nm * std::sin(ToRad(leeway_dir_deg));
        leeway_north_nm += leeway_step_nm * std::cos(ToRad(leeway_dir_deg));
      }

      if (current.available && leeway) {
        CombineVectors(current.speed_kt, current.direction_deg,
                       leeway_speed_kt, leeway_dir_deg, &drift_speed_kt,
                       &drift_dir_deg);
      } else if (current.available) {
        drift_speed_kt = current.speed_kt;
        drift_dir_deg = current.direction_deg;
      } else if (leeway) {
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
  result.leeway_nm = std::hypot(leeway_east_nm, leeway_north_nm);
  return result;
}

double TotalDriftVelocityErrorKt(double aswdve_kt, double twce_kt,
                                 double lwe_kt) {
  return std::sqrt(aswdve_kt * aswdve_kt + twce_kt * twce_kt +
                   lwe_kt * lwe_kt);
}

double TotalProbableErrorNm(double initial_error_nm, double drift_error_nm) {
  return std::sqrt(initial_error_nm * initial_error_nm +
                   drift_error_nm * drift_error_nm);
}

double DivergenceOffsetNm(double leeway_nm, double divergence_deg) {
  return 2.0 * leeway_nm * std::sin(ToRad(divergence_deg) / 2.0);
}

DatumUncertainty ComputeDatumUncertainty(const AgedDatum& aged,
                                         CraftType craft,
                                         double initial_error_nm) {
  // Drift error per component: the wind's effect on drift (ASWDVe), the
  // water current (TWCe), the leeway (LWe).
  double aswdve = 0.0;
  double twce = kUnknownComponentErrorKt;
  double lwe = kUnknownComponentErrorKt;
  switch (aged.source) {
    case DriftSource::kNone:
    case DriftSource::kManual:
      // Nothing known, or the operator's own estimate of the total drift:
      // an entered set & drift has no data behind it, so it must not come
      // out tighter than a GRIB does -- both components stay unknown. (It
      // still moves the datum; it just doesn't narrow the circle.)
      break;
    case DriftSource::kGrib:
      if (aged.wind_every_step && aged.leeway_applied) {
        aswdve = kKnownComponentErrorKt;
        lwe = kKnownComponentErrorKt;
      }
      if (aged.current_every_step) twce = kKnownComponentErrorKt;
      break;
  }

  DatumUncertainty u;
  u.initial_error_nm = initial_error_nm;
  u.dve_kt = TotalDriftVelocityErrorKt(aswdve, twce, lwe);
  const double hours = aged.elapsed.GetSeconds().ToDouble() / 3600.0;
  u.drift_error_nm = hours * u.dve_kt;
  u.e_nm = TotalProbableErrorNm(initial_error_nm, u.drift_error_nm);
  u.divergence_nm = DivergenceOffsetNm(
      aged.leeway_nm, LookupLeewayCoefficients(craft).divergence_deg);
  u.radius_nm = u.e_nm + u.divergence_nm;
  return u;
}
