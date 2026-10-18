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
