/******************************************************************************
 * Intercept plugin for OpenCPN -- standalone test for datum ageing.
 *
 * Builds without the OpenCPN plugin SDK or the wxWidgets GUI toolkit, same
 * as test_grib_reader.cpp: links datum_age.cpp and grib_reader.cpp directly
 * and only needs wxBase (wxFileName, wxDateTime, wxString, wxTimeSpan), not
 * wxCore. See CMakeLists.txt.
 *
 * Copyright (C) 2026 momo
 * License: GPLv3+  (see COPYING)
 ******************************************************************************/

#include "datum_age.h"

#include "grib_reader.h"

#include <wx/datetime.h>
#include <wx/filename.h>

#include <cmath>
#include <cstdio>
#include <ctime>

namespace {

int g_failures = 0;

void Check(bool cond, const char* what) {
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", what);
    ++g_failures;
  } else {
    std::printf("ok:   %s\n", what);
  }
}

wxString JoinPath(const wxString& dir, const wxString& file) {
  wxString d = dir;
  if (!d.IsEmpty() && d.Last() != wxFileName::GetPathSeparator()) {
    d += wxFileName::GetPathSeparator();
  }
  return d + file;
}

}  // namespace

int main(int argc, char** argv) {
  // The fixture directory is passed in by CMake (see add_test in
  // CMakeLists.txt) so this doesn't depend on ctest's working directory;
  // default to a path relative to a manual invocation from the repo root.
  wxString data_dir = (argc > 1) ? wxString(argv[1]) : wxString("test/data");

  // (1) No GRIB, no manual set & drift: the aged datum must equal the
  // reported position exactly (ComputeAgedDatum returns before the
  // integration loop runs any arithmetic on it), with elapsed time still
  // computed correctly.
  {
    const double lat = 36.5, lon = -5.5;  // Strait of Gibraltar area.
    const wxDateTime time_of_report(static_cast<time_t>(0));
    const wxDateTime now(static_cast<time_t>(7200));  // +2 hours.

    AgedDatum aged = ComputeAgedDatum(
        lat, lon, time_of_report, now, CraftType::kWooden,
        /*grib=*/nullptr, ManualSetAndDrift());

    Check(aged.lat == lat, "zero-drift: aged lat equals reported lat");
    Check(aged.lon == lon, "zero-drift: aged lon equals reported lon");
    Check(aged.elapsed.GetSeconds().ToLong() == 7200,
          "zero-drift: elapsed is 2 hours");

    // No drift data: current and leeway both unknown (0.5 kt each), so the
    // circle grows with time even though the datum doesn't move.
    Check(aged.source == DriftSource::kNone, "zero-drift: source is none");
    DatumUncertainty u =
        ComputeDatumUncertainty(aged, CraftType::kWooden, 1.0);
    const double dve = std::sqrt(0.5 * 0.5 + 0.5 * 0.5);
    Check(std::fabs(u.dve_kt - dve) < 1e-9,
          "zero-drift uncertainty: DVe from two unknown components");
    Check(std::fabs(u.drift_error_nm - 2.0 * dve) < 1e-9,
          "zero-drift uncertainty: De = 2 h * DVe");
    Check(std::fabs(u.e_nm - std::sqrt(1.0 + 4.0 * dve * dve)) < 1e-9,
          "zero-drift uncertainty: E = sqrt(X^2 + De^2)");
    Check(u.divergence_nm == 0.0 && u.radius_nm == u.e_nm,
          "zero-drift uncertainty: no leeway, so no divergence term");
  }

  // (2) Manual set & drift, no GRIB: due-east drift is a degenerate case of
  // the rhumb-line integration (bearing 90 keeps latitude fixed and turns
  // into a plain departure/cos(lat) longitude change), so the expected
  // position can be predicted in closed form rather than approximated.
  {
    // Matches kEarthRadiusNm in datum_age.cpp.
    const double kEarthRadiusNm = 3440.065;

    const double lat = 45.0, lon = 10.0;
    const wxDateTime time_of_report(static_cast<time_t>(0));
    const wxDateTime now(static_cast<time_t>(3600));  // +1 hour, two steps.

    ManualSetAndDrift manual;
    manual.available = true;
    manual.set_deg = 90.0;  // Due east.
    manual.drift_kt = 5.0;

    AgedDatum aged = ComputeAgedDatum(
        lat, lon, time_of_report, now, CraftType::kWooden,
        /*grib=*/nullptr, manual);

    const double distance_nm = manual.drift_kt * 1.0;  // 1 hour at 5 kt.
    const double expected_lon =
        lon + (distance_nm / kEarthRadiusNm) * (180.0 / M_PI) /
                  std::cos(lat * M_PI / 180.0);

    Check(std::fabs(aged.lat - lat) < 1e-6,
          "manual: due-east drift leaves latitude unchanged");
    Check(std::fabs(aged.lon - expected_lon) < 1e-6,
          "manual: due-east drift matches predicted longitude");
    Check(aged.elapsed.GetSeconds().ToLong() == 3600,
          "manual: elapsed is 1 hour");

    // An entered set & drift is the operator's estimate, not data: both
    // components stay unknown, so it is never tighter than a GRIB.
    Check(aged.source == DriftSource::kManual, "manual: source is manual");
    DatumUncertainty u =
        ComputeDatumUncertainty(aged, CraftType::kWooden, 1.0);
    Check(std::fabs(u.dve_kt - std::sqrt(0.5 * 0.5 + 0.5 * 0.5)) < 1e-9,
          "manual uncertainty: both components unknown");
    Check(u.divergence_nm == 0.0,
          "manual uncertainty: no separate leeway, so no divergence term");

    AgedDatum full_grib = aged;
    full_grib.source = DriftSource::kGrib;
    full_grib.wind_every_step = true;
    full_grib.current_every_step = true;
    Check(u.dve_kt >
              ComputeDatumUncertainty(full_grib, CraftType::kUnknown, 1.0).dve_kt,
          "manual uncertainty: wider than a GRIB with wind and current");
  }

  // (3) GRIB-sourced: fixture (a) from the GribReader test (10 m wind only,
  // no current) supplies the wind that drives leeway. Ageing exactly one
  // 30-minute integration step keeps the expected displacement a single,
  // independently-computable flat-earth approximation of the production
  // rhumb-line step -- close enough over a few nm that a loose tolerance
  // still catches a wiring bug (wrong direction, wrong time base, wrong
  // scaling) without duplicating the production math.
  {
    wxFileName fixture(JoinPath(data_dir, "gfs_10m_wind_simple_drt5.0.grib2"));
    if (!fixture.FileExists()) {
      std::printf(
          "SKIP: fixture gfs_10m_wind_simple_drt5.0.grib2 not present "
          "(see test/data/README.md)\n");
    } else {
      GribReader grib(fixture);
      // Grid is 30-40N/10-20E at 0.25 degree spacing; (35, 15) lands
      // exactly on a grid point. Same cycle time as the GribReader test.
      const double lat = 35.0, lon = 15.0;
      const wxDateTime kValidTime(static_cast<time_t>(1788220800));
      const wxDateTime now = kValidTime + wxTimeSpan::Minutes(30);
      const CraftType craft_type = CraftType::kRubber;

      EnvSample wind = grib.LookupWind(lat, lon, kValidTime);
      Check(wind.available,
            "GRIB-sourced: precondition -- wind sample available at fixture "
            "point");

      AgedDatum aged =
          ComputeAgedDatum(lat, lon, kValidTime, now, craft_type, &grib,
                            ManualSetAndDrift());

      LeewayCoefficients coeffs = LookupLeewayCoefficients(craft_type);
      double leeway_speed_kt =
          coeffs.speed_pct_of_wind * wind.speed_kt + coeffs.speed_constant_kt;
      double leeway_dir_deg = std::fmod(wind.direction_deg + 180.0, 360.0);
      Check(leeway_speed_kt > 0.0,
            "GRIB-sourced: precondition -- fixture wind yields nonzero "
            "leeway");

      double distance_nm = leeway_speed_kt * 0.5;  // 30 minutes.
      double bearing_rad = leeway_dir_deg * M_PI / 180.0;
      double expected_lat = lat + (distance_nm / 60.0) * std::cos(bearing_rad);
      double expected_lon =
          lon + (distance_nm / 60.0) * std::sin(bearing_rad) /
                    std::cos(lat * M_PI / 180.0);

      Check(std::fabs(aged.lat - expected_lat) < 0.01,
            "GRIB-sourced: aged lat matches leeway-only prediction");
      Check(std::fabs(aged.lon - expected_lon) < 0.01,
            "GRIB-sourced: aged lon matches leeway-only prediction");
      Check(aged.elapsed.GetSeconds().ToLong() == 1800,
            "GRIB-sourced: elapsed is 30 minutes");

      // The fixture has wind but no current: wind known, current unknown.
      Check(aged.source == DriftSource::kGrib, "GRIB-sourced: source is GRIB");
      Check(aged.wind_every_step, "GRIB-sourced: wind at every step");
      Check(!aged.current_every_step,
            "GRIB-sourced: no current in this fixture");
      Check(std::fabs(aged.leeway_nm - distance_nm) < 1e-9,
            "GRIB-sourced: leeway distance = leeway speed * 30 min");
      DatumUncertainty u = ComputeDatumUncertainty(aged, craft_type, 1.0);
      Check(std::fabs(u.dve_kt -
                      std::sqrt(0.3 * 0.3 + 0.5 * 0.5 + 0.3 * 0.3)) < 1e-9,
            "GRIB-sourced uncertainty: missing current counts as unknown");
      Check(std::fabs(u.divergence_nm -
                      2.0 * distance_nm * std::sin(15.0 * M_PI / 180.0)) <
                1e-9,
            "GRIB-sourced uncertainty: +/-30 deg divergence for a rubber boat");
      Check(std::fabs(u.radius_nm - (u.e_nm + u.divergence_nm)) < 1e-12,
            "GRIB-sourced uncertainty: radius = E + divergence");

      // Leeway off: this fixture has no current, so nothing moves the
      // target -- and the leeway, not estimated, counts as unknown.
      AgedDatum no_lw = ComputeAgedDatum(lat, lon, kValidTime, now, craft_type,
                                         &grib, ManualSetAndDrift(),
                                         /*include_leeway=*/false);
      Check(no_lw.lat == lat && no_lw.lon == lon,
            "leeway off: wind-only GRIB leaves the datum at the report");
      Check(!no_lw.leeway_applied && no_lw.leeway_nm == 0.0,
            "leeway off: no leeway applied, no leeway distance");
      DatumUncertainty u_off = ComputeDatumUncertainty(no_lw, craft_type, 1.0);
      Check(std::fabs(u_off.dve_kt - std::sqrt(0.5 * 0.5 + 0.5 * 0.5)) < 1e-9,
            "leeway off: leeway (and the missing current) unknown");
      Check(u_off.dve_kt > u.dve_kt,
            "leeway off: a wider circle than with leeway, not a tighter one");
    }
  }

  // (4) Zero elapsed time: time_of_report == now, with a manual drift
  // available. The datum must equal the reported position exactly --
  // ComputeAgedDatum's now <= time_of_report guard returns before the
  // integration loop ever runs, regardless of what drift source is set.
  {
    const double lat = 51.0, lon = -1.0;
    const wxDateTime time_of_report(static_cast<time_t>(1000));
    const wxDateTime now(static_cast<time_t>(1000));  // Same instant.

    ManualSetAndDrift manual;
    manual.available = true;
    manual.set_deg = 45.0;
    manual.drift_kt = 3.0;

    AgedDatum aged = ComputeAgedDatum(
        lat, lon, time_of_report, now, CraftType::kWooden,
        /*grib=*/nullptr, manual);

    Check(aged.lat == lat, "zero-elapsed: aged lat equals reported lat");
    Check(aged.lon == lon, "zero-elapsed: aged lon equals reported lon");
    Check(aged.elapsed.GetSeconds().ToLong() == 0,
          "zero-elapsed: elapsed is zero");
  }

  // (5) Southward manual drift entered as set_deg = -180, which
  // NormalizeDegrees must fold into a true bearing of 180 (due south)
  // before it drives the integration. Due south is the mirror of the
  // due-east case: a degenerate rhumb line that keeps longitude fixed, so
  // the expected latitude change can be predicted in closed form.
  {
    const double kEarthRadiusNm = 3440.065;

    const double lat = 45.0, lon = 10.0;
    const wxDateTime time_of_report(static_cast<time_t>(0));
    const wxDateTime now(static_cast<time_t>(3600));  // +1 hour.

    ManualSetAndDrift manual;
    manual.available = true;
    manual.set_deg = -180.0;  // Normalises to 180 (due south).
    manual.drift_kt = 5.0;

    AgedDatum aged = ComputeAgedDatum(
        lat, lon, time_of_report, now, CraftType::kWooden,
        /*grib=*/nullptr, manual);

    const double distance_nm = manual.drift_kt * 1.0;  // 1 hour at 5 kt.
    const double expected_lat =
        lat - (distance_nm / kEarthRadiusNm) * (180.0 / M_PI);

    Check(std::fabs(aged.lat - expected_lat) < 1e-6,
          "manual south: set_deg=-180 normalises and matches predicted "
          "latitude");
    Check(std::fabs(aged.lon - lon) < 1e-6,
          "manual south: due-south drift leaves longitude unchanged");
  }

  // (6) High-latitude start (>70N): due-east manual drift, same closed
  // form as case (2) but far enough poleward to actually exercise the
  // meridian-convergence term (q) in AdvancePosition's rhumb-line math
  // rather than the near-equatorial case where it barely matters.
  {
    const double kEarthRadiusNm = 3440.065;

    const double lat = 75.0, lon = 20.0;
    const wxDateTime time_of_report(static_cast<time_t>(0));
    const wxDateTime now(static_cast<time_t>(3600));  // +1 hour.

    ManualSetAndDrift manual;
    manual.available = true;
    manual.set_deg = 90.0;  // Due east.
    manual.drift_kt = 5.0;

    AgedDatum aged = ComputeAgedDatum(
        lat, lon, time_of_report, now, CraftType::kWooden,
        /*grib=*/nullptr, manual);

    const double distance_nm = manual.drift_kt * 1.0;  // 1 hour at 5 kt.
    const double expected_lon =
        lon + (distance_nm / kEarthRadiusNm) * (180.0 / M_PI) /
                  std::cos(lat * M_PI / 180.0);

    Check(std::fabs(aged.lat - lat) < 1e-6,
          "high-latitude: due-east drift leaves latitude unchanged");
    Check(std::fabs(aged.lon - expected_lon) < 1e-6,
          "high-latitude: due-east drift matches predicted longitude above "
          "70N");
  }

  // (7) CombineVectors zero-resultant guard: a current vector and a leeway
  // vector that cancel sum to the zero vector, which used to fall through
  // to atan2(0, 0) -- defined, but a platform convention rather than a
  // meaningful bearing. Zero-magnitude inputs are used rather than two
  // equal-and-opposite nonzero vectors (e.g. 5 kt at 90 deg and 5 kt at
  // 270 deg): multiplying by an exact 0.0 speed is guaranteed by IEEE 754
  // to sum to exactly 0.0 on every platform and optimisation level, while
  // two nonzero vectors 180 deg apart only cancel to *approximately* zero
  // -- sin/cos of independently-rounded angles leave a residual of a few
  // ULP that is sensitive to the libm implementation and even to
  // surrounding code changing how the compiler schedules the FP ops (this
  // was confirmed directly: the same nonzero-vector case flipped between
  // failing and passing here after an unrelated debug print changed
  // inlining). A reliably exact zero resultant needs zero-magnitude
  // vectors, which still exercises the same `x == 0 && y == 0` branch.
  {
    double speed_kt = -1.0, dir_deg = -1.0;
    CombineVectors(0.0, 90.0, 0.0, 270.0, &speed_kt, &dir_deg);
    Check(speed_kt == 0.0, "CombineVectors: zero-magnitude vectors -> zero speed");
    Check(dir_deg == 0.0, "CombineVectors: zero-magnitude vectors -> zero dir");
  }

  if (g_failures > 0) {
    std::fprintf(stderr, "%d check(s) failed\n", g_failures);
    return 1;
  }
  std::printf("All checks passed.\n");
  return 0;
}
