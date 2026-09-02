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
        lat, lon, time_of_report, now, "Wooden boat (displacement hull)",
        /*grib=*/nullptr, ManualSetAndDrift());

    Check(aged.lat == lat, "zero-drift: aged lat equals reported lat");
    Check(aged.lon == lon, "zero-drift: aged lon equals reported lon");
    Check(aged.elapsed.GetSeconds().ToLong() == 7200,
          "zero-drift: elapsed is 2 hours");
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
        lat, lon, time_of_report, now, "Wooden boat (displacement hull)",
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
      const wxString craft_type = "Rubber boat (inflatable, RIB, liferaft)";

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
    }
  }

  if (g_failures > 0) {
    std::fprintf(stderr, "%d check(s) failed\n", g_failures);
    return 1;
  }
  std::printf("All checks passed.\n");
  return 0;
}
