/******************************************************************************
 * Intercept plugin for OpenCPN -- standalone test for number formatting.
 *
 * Builds without the OpenCPN plugin SDK or wxWidgets: everything in
 * format.h is plain C++ over doubles and std::string. See CMakeLists.txt.
 *
 * Copyright (C) 2026 momo
 * License: GPLv3+  (see COPYING)
 ******************************************************************************/

#include "format.h"

#include <cstdio>
#include <cstring>

namespace {

int g_failures = 0;

void CheckEq(const std::string& actual, const char* expected,
             const char* what) {
  if (actual != expected) {
    std::fprintf(stderr, "FAIL: %s (got \"%s\", want \"%s\")\n", what,
                 actual.c_str(), expected);
    ++g_failures;
  } else {
    std::printf("ok:   %s\n", what);
  }
}

}  // namespace

int main() {
  // -------- DD --------
  CheckEq(FormatLatDD(34.5), "34.500000 N", "DD: positive lat -> N");
  CheckEq(FormatLatDD(-33.867), "33.867000 S", "DD: negative lat -> S");
  CheckEq(FormatLatDD(0.0), "0.000000 N", "DD: zero lat -> N");
  CheckEq(FormatLonDD(151.207), "151.207000 E", "DD: positive lon -> E");
  CheckEq(FormatLonDD(-118.25), "118.250000 W", "DD: negative lon -> W");
  CheckEq(FormatLonDD(0.0), "0.000000 E", "DD: zero lon -> E");

  // -------- DDM --------
  CheckEq(FormatLatDDM(34.5), "34 30.000' N", "DDM: positive lat");
  CheckEq(FormatLatDDM(-33.5), "33 30.000' S", "DDM: negative lat -> S");
  CheckEq(FormatLonDDM(-118.75), "118 45.000' W", "DDM: negative lon -> W");
  CheckEq(FormatLonDDM(0.0), "0 00.000' E", "DDM: zero lon -> E");

  // -------- DMS --------
  // 34.5125 deg = 34 deg, 0.5125*60 = 30.75 min = 30 min, 0.75*60 = 45.0 sec.
  CheckEq(FormatLatDMS(34.5125), "34 30 45.0\" N", "DMS: positive lat");
  CheckEq(FormatLatDMS(-34.5125), "34 30 45.0\" S", "DMS: negative lat -> S");
  CheckEq(FormatLonDMS(-118.75), "118 45 00.0\" W", "DMS: negative lon -> W");
  CheckEq(FormatLonDMS(0.0), "0 00 00.0\" E", "DMS: zero lon -> E");

  // -------- Bearing --------
  CheckEq(FormatBearingDeg(0.0), "0.0", "bearing: zero");
  CheckEq(FormatBearingDeg(45.04), "45.0", "bearing: plain rounding");
  CheckEq(FormatBearingDeg(360.0), "0.0", "bearing: 360 -> 0");
  CheckEq(FormatBearingDeg(359.96), "0.0",
          "bearing: rounds up into wraparound");
  CheckEq(FormatBearingDeg(-10.0), "350.0", "bearing: negative wraps");

  // -------- Distance --------
  CheckEq(FormatDistanceNm(0.0), "0.0", "distance: zero");
  CheckEq(FormatDistanceNm(12.34), "12.3", "distance: rounds to 1 decimal");
  CheckEq(FormatDistanceNm(5.06), "5.1", "distance: rounds up");

  // -------- ETA --------
  CheckEq(FormatEtaHhMm(0.0), "00:00", "eta: zero");
  CheckEq(FormatEtaHhMm(1.5), "01:30", "eta: 1.5h");
  CheckEq(FormatEtaHhMm(25.75), "25:45", "eta: past 24h stays as elapsed");
  CheckEq(FormatEtaHhMm(1.0 / 60.0), "00:01", "eta: one minute");

  if (g_failures > 0) {
    std::fprintf(stderr, "%d check(s) failed\n", g_failures);
    return 1;
  }
  std::printf("All checks passed.\n");
  return 0;
}
